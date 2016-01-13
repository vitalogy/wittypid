# wittypid

#### daemon for the [Witty Pi](http://www.uugear.com/product/witty-pi-realtime-clock-and-power-management-for-raspberry-pi/ "Witty Pi by UUGear") (extension board for the Raspberry Pi)

* you can shutdown the Raspberry with one button push
* or you can restart your Raspberry with a second push 

The wittypid (Witty Pi daemon) reacts on an interrupt on the GPIO that is defined with HALT_GPIO.
The default HALT_GPIO is wiringX (also wiringPi) GPIO 7 (BCM GPIO 4).
For the time the daemon is waiting that the gpio goes stable, the led trigger for the LED (default wittypi_led in sysfs defined in [wittypi-overlay.dts](https://github.com/raspberrypi/linux/blob/rpi-4.4.y/arch/arm/boot/dts/overlays/wittypi-overlay.dts))
is set to heartbeat (customizeable), so the LED on the Witty Pi board is flashing. 
After the GPIO goes stable (at least wittypid watch the GPIO for 20 seconds), the led trigger
is set to default-on (customizeable). Now the daemon waits for an interrupt on the HALT_GPIO.
If you push the button on Witty Pi board only once, the system will shutdown.
By a second push within 2 seconds the system will reboot.

files:
* src/wittypid.c - wittypi daemon source code
* systemd/wittypid.service - systemd service file
* wittypid.conf - config file for wittypid (default location /etc)

You should use kernel rpi-4.4y or later, so the patches are included and the dtbo file for the Witty Pi is present.

#### compile wittypid.c (you need wiringX and libconfuse)
type ```make``` and ```make install```

or manual ```gcc wittypid.c -o wittypid -Wall -lwiringX -lconfuse```
and copy the necessary files to the default locations
