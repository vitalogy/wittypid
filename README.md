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

You should use kernel rpi-4.4y or later, so the dtbo file for the Witty Pi is present and the patches to use the RTC as wakeup-source are included.
To use the RTC you can enable the wittypi-overlay in ```/boot/config.txt```.
For example: dtoverlay=wittypi (Please have also a look at [this](https://github.com/raspberrypi/linux/blob/rpi-4.4.y/arch/arm/boot/dts/overlays/README ""))

#### compile wittypid.c (you need wiringX and libconfuse)
type ```make``` to compile from source
and ```make install``` or ```make install_all``` as root to install the files

or manual ```gcc wittypid.c -o wittypid -Wall -lwiringX -lconfuse```
and copy the necessary files to the default locations

#### Arch Linux ARM ####

* build from source with makepkg [PKGBUILD](https://github.com/vitalogy/myPKGBUILDs/tree/master/wittypid "PKGBUILD")
* or download pre-compiled [wittypid-0.5-0-armv7h.pkg.tar.xz](http://milaw.biz/files/wittypid-0.5-0-armv7h.pkg.tar.xz "wittypid-0.5-0-armv7h.pkg.tar.xz") and install with pacman
  * $ ```cd /tmp && wget http://milaw.biz/files/wittypid-0.5-0-armv7h.pkg.tar.xz```
  * $ ```su```
  * # ```pacman -U wittypid-0.5-0-armv7h.pkg.tar.xz```
