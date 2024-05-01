# IECDevice

This library provides an interface to connect a variety of current 
microcontrollers to the Commodre IEC bus used on the C64, C128 and VIC-20.

The library provides one class (IECDevice) for low-level bus access and 
another (IECFileDevice) to easily implement file-based devices. The **JiffyDos**
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
  - [IECFDC](examples/IECFDC)/[IECFDCMega](examples/IECFDCMega) combines this library with my ArduinoFDC library to connect regular (MFM) floppy disk drives to the IEC bus.

# Installation

To install this library, click the green "Code" button on the top and select "Download ZIP".
Then in the Arduino IDE select "Sketch->Include Library->Add ZIP Library" and select the downloaded ZIP file.
After that you will find the included examples in File->Examples->IECDevice->...".
