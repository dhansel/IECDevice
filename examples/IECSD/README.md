# IECSD

This example implements an SD card reader for the IEC bus using the IECFileDevice class.
While it does support a usable subset of [features](#supported-functionality-and-limitations),
it is not meant as a replacement for existing solutions ([SD2IEC](https://www.c64-wiki.com/wiki/SD2IEC))
but rather a demonstration of this library.

As is, this device will show up as device #9 on the IEC bus. The device number can be changed
by altering the `#define DEVICE_NUMBER 9` line in IECSD.ino.

## Wiring

The following table lists the IEC bus pin connections for this example for different 
microcontrollers (NC=not connected):

IEC Bus Pin | Signal   | Arduino Uno | Mega | Micro | Due | Raspberry Pi Pico | ESP32
------------|----------|-------------|------|-------|-----|-------------------|------
1           | SRQ      | NC          | NC   | NC    | NC  | NC                | NC 
2           | GND      | GND         | GND  | GND   | GND | GND               | GND
3           | ATN      | 3           | 3    | 3     | 3   | GPIO2             | IO34
4           | CLK      | 4           | 4    | 4     | 4   | GPIO3             | IO32
5           | DATA     | 5           | 5    | 5     | 5   | GPIO4             | IO33
6           | RESET    | 6           | 6    | 6     | 6   | GPIO5             | IO35

When looking at the IEC bus connector at the back of your Commodore, the pins are as follows:  
<img src="../../IECBusPins.jpg" width="20%">   

The example uses the following pins for the SPI connections to the SD card module
(*=use on-board 6-pin SPI header):

Signal | Arduino Uno | Mega | Mico | Due | Raspberry Pi Pico | ESP32
-------|-------------|------|------|-----|-------------------|------
SCK    | 13/*        | 52   | 15   |  *  |  GPIO18           | IO18
MISO   | 12/*        | 50   | 14   |  *  |  GPIO16           | IO19
MOSI   | 11/*        | 51   | 16   |  *  |  GPIO19           | IO23
CS     | 10          | 10   | 10   | 10  |  GPIO17           | IO5


An activity LED that turns on during data transfers can be connected on the following pins
(*=uses on-board LED):

Arduino Uno | Mega | Mico | Due  | Raspberry Pi Pico | ESP32
------------|------|------|------|-------------------|------
10          | 13/* |  *   | 13/* |  *                | IO21

As described in the Wiring section for the IECDevice library, controllers running
at 5V (Arduino Uno, Mega or Micro) can be connected directly to the IEC bus.
Controllers running at 3.3V (Arduino Due, Raspberry Pi Pico or ESP32) require a 
[voltage level converter](https://www.sparkfun.com/products/12009).

To support DolphinDos fast transmissions via a parallel cable, connect the cable as
described in the DolphinDos section for the IECDevice library.

Note that the IEC bus does supply 5V power so you will need to power
your device either from an external 5V supply or use the 5V output available on
the computer's user port, cassette port or expansion port.

## Supported functionality and limitations

This is a simple example meant to demonstrate the IECFileDevice class. It is not intended
to be a full-featured floppy disk replacement. Other solutions already exist for this purpose 
(e.g. [SD2IEC](https://www.c64-wiki.com/wiki/SD2IEC)).

This example supports:
  - Listing directory via LOAD"$",9
  - Loading and saving files (LOAD and SAVE commands)
  - Reading and writing data via the OPEN/PRINT#/INPUT# BASIC commands
  - Reading the device status (channel 15)
  - Deleting files by sending a "S:filename" DOS command on the command channel (channel 15)
  - Creating ("MD:name"), changing ("CD:name") and deleting ("RD:name") directories via sending commands on the DOS command channel.
  - Fast data transfer using JiffyDos, Epyx FastLoad and DolphinDos

Limitations:
  - Only one file can be opened at a time
  - Only the SCRATCH (S:) DOS command is supported

If you would like to be able to use Commodore disk image files (D64, G64 etc) then
install my [VDrive library](https://github.com/dhansel/VDrive) and un-comment the
```#define HAVE_VDRIVE``` line in the ```IECSD.h``` file. Note that due to the memory
requirements this only works on 32-bit architectures (Pi Pico, ESP32 or Arduino Due).
