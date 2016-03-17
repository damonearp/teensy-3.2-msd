# teensy-3.2-msd
Custom teensy firmware to have the teensy act as a USB Mass Storage Device. 

This is a learning project and the goal was to create a USB MSD with a Teensy 3.2 and the Micro SD card reader. While the code relys on existing Teensy and Arduino code, most of the dependencies have been removed and the USB driver updated/rewritten. As such it is a standalone project which only needs the gcc arm-none-eabi build tools to make and [PaulStoffregen/teensy_loader_cli](https://github.com/PaulStoffregen/teensy_loader_cli) to load onto the Teensy.

Kevin Cuzner provided excellent information in his [Teensy 3.1 Bare Metal Post](http://kevincuzner.com/2014/12/12/teensy-3-1-bare-metal-writing-a-usb-driver) and sample code at [kcuzner/teensy-oscilloscope](https://github.com/kcuzner/teensy-oscilloscope).

The final device is a [Teensy 3.2](https://www.pjrc.com/store/teensy32.html) soldered together with a [Micro SD Card Adapter](https://www.pjrc.com/store/wiz820_sd_adaptor.html), and a Micro SD card for storage. It provides logging over the first serial tx pin, and has been rudementarly tested with Linux v4.4.x and Windows 7.

Code Dependencies
- [PaulStoffregen/cores](https://github.com/PaulStoffregen/cores) 
- [PaulStoffregen/SPI](https://github.com/PaulStoffregen/SPI)
- [adafruit/SD](https://github.com/adafruit/SD)
