/*
 * Witty Pi daemon (wittypid)
 *
 *
 * Copyright (C) 2016, Michael Lange.
 * Author : Michael Lange <linuxstuff@milaw.biz>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/reboot.h>
#include "confuse.h"
#include "wiringX.h"

#define CONFIGFILE	"/etc/wittypid.conf"	// default config location

volatile sig_atomic_t keep_going = 1;
volatile sig_atomic_t regular_stop = 0;


void sleep_ms(int milliseconds) {
	struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int set_halt_gpio(int haltgpio) {
	int ret = 1;

	ret = wiringXISR(haltgpio, INT_EDGE_FALLING);
	if (ret != 0) {
        syslog(LOG_ERR, "Setting GPIO %d to INT_EDGE_FALLING failed\n", haltgpio);
	}
	return ret;
}

void set_led_trigger(char *led, char *trigger) {
	FILE *f = NULL;
	char path[80];

	sprintf(path, "/sys/class/leds/%s/trigger", led);
	if ((f = fopen(path, "w")) == NULL) {
		syslog(LOG_ERR, "Unable to write to %s", path);
	} else {
		fprintf(f, "%s\n", trigger);
		fclose(f);
	}
}

int button_is_pressed(int gpio, int ms) {
	int ret = 0;

	sleep_ms(ms);
	if (digitalRead(gpio) == 0) {
		ret = 1;
	} else {
		syslog(LOG_INFO, "Button was pressed to short, or short pull down from system\n");
	}
	return ret;
}

void sighandler(int signum, siginfo_t *siginfo, void *ptr) {
	syslog(LOG_INFO, "Received signal %d, from process %lu\n", signum, (unsigned long)siginfo->si_pid);
	keep_going = 0;
	regular_stop = 1;
}

cfg_t *parse_conf(const char *filename) {
	cfg_opt_t opts[] = {
		CFG_STR("LED_NAME", "wittypi_led", CFGF_NONE),
		CFG_STR("LED_TRIGGER_READY", "default-on", CFGF_NONE),
		CFG_STR("LED_TRIGGER_STOP", "heartbeat", CFGF_NONE),
		CFG_INT("HALT_GPIO", 7, CFGF_NONE),
		CFG_INT("STABLE_DELAY", 20, CFGF_NONE),
		CFG_END()
	};

	cfg_t *cfg = cfg_init(opts, CFGF_NONE);

	switch (cfg_parse(cfg, filename)) {
		case CFG_FILE_ERROR:
			syslog(LOG_WARNING, "Failed to read the configuration file '%s'\n", filename);
			syslog(LOG_WARNING, "Using the default values...\n");
		case CFG_SUCCESS:
			break;
		case CFG_PARSE_ERROR:
			return 0;
	}

	return cfg;
}

int main(int argc, char *argv[]) {
	pid_t pid, sid;
	struct sigaction act;
	int i = 0;
	int gpio = 0;
	int stabletime = 0;
	int buttonpressed = 0;
	int gpio_stable = 0;

	/* Get the configuration with libconfuse */
	cfg_t *cfg = parse_conf(argc > 1 ? argv[1] : CONFIGFILE);
	if (!cfg) {
		syslog(LOG_ERR, "Could not initialize configuration");
		exit(EXIT_FAILURE);
	}

	/* Get the HALT_GPIO and check if valid */
	gpio = cfg_getint(cfg, "HALT_GPIO");
	wiringXSetup();
	if (wiringXValidGPIO(gpio) != 0) {
		syslog(LOG_ERR, "Invalid HALT_GPIO %d in config file!\n", gpio);
		exit(EXIT_FAILURE);
	}

	/* Set led trigger to "none" */
	set_led_trigger(cfg_getstr(cfg, "LED_NAME"), "none");

	/*
	 * Clone ourselves
	 * pid < 0 - failure when forking
	 * pid > 0 - forking was succesfull
	 * pid = 0 - we are in the child process
	 */
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Set the umask to zero */
	umask(0);

	/* Open connection to the syslog server */
	openlog(NULL, LOG_PID, LOG_USER);

	/* Try to create our own process group */
	sid = setsid();
	if (sid < 0) {
		syslog(LOG_ERR, "Could not create process group\n");
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		syslog(LOG_ERR, "Could not change working directory to /\n");
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Handle sigaction */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGTERM, &act, NULL);

	/*
	 * If the HALT_GPIO could be set to an interrupt pin, then start
	 * flashing the LED and wait for stable HALT_GPIO (try this ten times).
	 * The HALT_GPIO must be stable for time set to STABLE_DELAY (default 20 seconds).
	 */
	if (set_halt_gpio(gpio) == 0) {
		stabletime = ((int)(cfg_getint(cfg, "STABLE_DELAY")) * 1000);
		syslog(LOG_INFO, "Waiting for stable GPIO %d for %d seconds\n", gpio, stabletime / 1000);
		i = 0;
		while (keep_going) {
			if (i == 10) {
				syslog(LOG_ERR, "GPIO %d is not stable, tried this %d times\n", gpio, i);
				break;
			}
			set_led_trigger(cfg_getstr(cfg, "LED_NAME"), "heartbeat");
			if (waitForInterrupt(gpio, stabletime) > 0) {
				syslog(LOG_WARNING, "Got an interrupt, return for waiting for a stable GPIO %d. try: %d/10\n", gpio, i+1);
				set_led_trigger(cfg_getstr(cfg, "LED_NAME"), "none");
				sleep_ms(3000);
				++i;
			} else {
				gpio_stable = 1;
				break;
			}
		}

		/*
		 * When the HALT_GPIO is stable, then log this,
		 * restore the GPIO, set the led trigger
		 * and wait for someone to push the button.
		 */
		if (gpio_stable) {
			syslog(LOG_INFO, "GPIO %d is stable\n", gpio);
			set_halt_gpio(gpio);
			set_led_trigger(cfg_getstr(cfg, "LED_NAME"), cfg_getstr(cfg, "LED_TRIGGER_READY"));
			syslog(LOG_INFO, "Waiting for someone to push the button ...\n");
			while (keep_going) {
				if (waitForInterrupt(gpio, 500) > 0) {
					if (button_is_pressed(gpio, 100) == 1) {
						syslog(LOG_INFO, "Button pressed once\n");
						buttonpressed = 1;
						keep_going = 0;
						regular_stop = 1;
					}
				}

			}

			/* If button is pressed again within 2 sec, then restart the system. */
			if (buttonpressed == 1) {
				set_halt_gpio(gpio);
				if (waitForInterrupt(gpio, 2000) > 0) {
					syslog(LOG_INFO, "Button pressed twice\n");
					buttonpressed = 2;
				}
			}

			/* Restore the GPIO */
			set_halt_gpio(gpio);
		}
	}

	/* Set LED trigger to none, if there was an error */
	if (regular_stop) {
		set_led_trigger(cfg_getstr(cfg, "LED_NAME"), cfg_getstr(cfg, "LED_TRIGGER_STOP"));
	} else {
		set_led_trigger(cfg_getstr(cfg, "LED_NAME"), "none");
	}

	/* Only terminating or reboot/shutdown the system? */
	if (buttonpressed == 0) {
		syslog(LOG_WARNING, "Terminating\n");
	} else if (buttonpressed == 1) {
		syslog(LOG_WARNING, "Shutting down system\n");
		system("shutdown -P now");
	} else if (buttonpressed == 2) {
		syslog(LOG_WARNING, "Rebooting system\n");
		system("reboot");
	}

	cfg_free(cfg);
	closelog();
	exit(EXIT_SUCCESS);
}
