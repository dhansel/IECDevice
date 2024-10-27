# IECBasicSD

This example demonstrates how to implement a very basic SD card reader using the IECDevice
class. It can be used to connect a microcontroller (e.g Arduino Uno) to the Commodore IEC bus and
read/write files on an SD card.

As is, this device will show up as device #9 on the IEC bus. The device number can be changed
by altering the `#define DEVICE_NUMBER 9` line in IECBasicSD.ino.

Note that since this device is derived from the IECFileDevice class, it will automatically 
support JiffyDos, Epyx FastLoad and DolphinDos fast load protocols, provided that the connected
C64 is fitted with the corresponding kernal/cartridge.

Note also that this device is very limited in its functionality, it allows loading and saving programs
on the SD card but no other functionality (directory listing, status channel, deleting files etc..).
The purpose of this example is to demonstrate the basic implementation of a file-based device using the
IECFileDevice class. A more feature-complete implementation of a SD card reader is provided in
the [IECSD example](../IECSD). 


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


As described in the Wiring section for the IECDevice library, controllers running
at 5V (Arduino Uno, Mega or Micro) can be connected directly to the IEC bus.
Controllers running at 3.3V (Arduino Due, Raspberry Pi Pico or ESP32) require a 
[voltage level converter](https://www.sparkfun.com/products/12009).

Note that the IEC bus does not supply 5V power so you will need to power
your device either from an external 5V supply or use the 5V output available on
the computer's user port, cassette port or expansion port.

## Using this sketch

After programming your microcontroller and connecting it up to the IEC bus
you can save a program to the SD card from the computer using the command
`SAVE "filename", 9` and load a file using `LOAD "filename", 9`.
