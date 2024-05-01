# IECDevice

This library for the Arduino IDE provides an interface to connect a variety of current 
microcontrollers to the Commodre IEC bus used on the C64, C128 and VIC-20.

The library provides one class (IECDevice) for low-level bus access and 
another (IECFileDevice) to implement file-based devices. The **JiffyDos**
protocol for fast data transfer is also supported.

IECDevice has (so far) been tested on the following microcontrollers:
  -  8-bit ATMega devices (Arduino Uno, Mega, Mini, Micro, Leonardo)
  -  Arduino Due
  -  ESP32
  -  Raspberry Pi Pico

The library comes with a number of examples:
  - [IECBasicSerial](examples/IECBasicSerial) demonstrates how to use the IECDevice class to implement a very simple IEC-Bus-to-serial converter.
  - [IECSD](examples/IECSD) demonstrates how to use the IECFileDevice class to implement a simple SD card reader
  - [IECCentronics](examples/IECCentronics) is a converter to connect Centronics printers to via the IEC bus
  - [IECFDC](examples/IECFDC)/[IECFDCMega](examples/IECFDCMega) combines this library with my [ArduinoFDC  library](https://github.com/dhansel/ArduinoFDC) to connect PC floppy disk drives (3.5" or 5") to the IEC bus.

# Installation

To install this library, click the green "Code" button on the top of this page and select "Download ZIP".
Then in the Arduino IDE select "Sketch->Include Library->Add ZIP Library" and select the downloaded ZIP file.
After doing so you will find the included examples in File->Examples->IECDevice->...".

# Wiring

For 5V platforms such as the Arduino Uno, the IEC bus signals (ATN, Clock, Data, Reset) can be directly 
connected to the Microcontroller. The pins can be freely chosen and are configured in the class 
constructor (see description below). It is recommended to choose an interrupt-capable pin for the ATN 
signal. 

For 3.3V platforms (Raspberry Pi Pico, ESP32, Arduino Due) a level shifter is required to isolate the
controller from the 5V signals on the IEC bus. I am using this [SparkFun level converter](https://www.sparkfun.com/products/12009) 
other models should do fine as the IEC bus is not particularly fast. Connect the IEC bus signals
plus 5V supply to the "High Voltage" side and microcontroller pins plus 3.3V supply to the "Low Voltage" side.

# Timing considerations

# Library functions
