# IECDevice

This library for the Arduino IDE provides a simple interface to connect a variety of current 
microcontrollers to the Commodre IEC bus used on the C64, C128 and VIC-20. This should
make it easier for hobbyists to create new devices for those computers.

The library provides two classes:
  - ```IECDevice``` for creating low-level bus devices that directly respond to the data 
    sent over the bus one byte at a time. This can be used to implement devices such as
    [printers](examples/IECCentronics) or [modem-like](examples/IECBasicSerial) devices.
  - ```IECFileDevice``` for creating higher-level devices that operate more like disk
    drives. The IECFileDevice interface is file-based, providing open/close/read/write functions.
    An example use for this class would be an [SD-card reader](examples/IECSD).
    Any device created using this class automatically supports the following fast load protocols:
    - [JiffyDos](#jiffydos-support),
    - [Epyx FastLoad](#epyx-fastload-support)
    - [DolphinDos](#dolphindos-support)

So far I have tested this library on the following microcontrollers:
  -  Arduino 8-bit ATMega devices (Uno R3, Mega, Mini, Micro, Leonardo)
  -  Arduino Uno R4
  -  Arduino Due (32-bit)
  -  ESP32
  -  Raspberry Pi Pico

A number of examples are included to demonstrate how to implement devices using the two classes:
  - [IECBasicSerial](examples/IECBasicSerial) demonstrates how to use the IECDevice class to implement a very simple IEC-Bus-to-serial converter.
  - [IECSD](examples/IECSD) demonstrates how to use the IECFileDevice class to implement a simple SD card reader
  - [IECCentronics](examples/IECCentronics) is a converter to connect Centronics printers to via the IEC bus
  - [IECFDC](examples/IECFDC)/[IECFDCMega](examples/IECFDCMega) combines this library with my [ArduinoFDC library](https://github.com/dhansel/ArduinoFDC) to connect PC floppy disk drives (3.5" or 5") to the IEC bus.

## Installation

To install this library, click the green "Code" button on the top of this page and select "Download ZIP".
Then in the Arduino IDE select "Sketch->Include Library->Add ZIP Library" and select the downloaded ZIP file.
After doing so you will find the included [examples](examples) in File->Examples->IECDevice->...".

For the Raspberry Pi Pico, this library has been tested within the Arduino IDE using the "Raspberry Pi RP2040 (4.1.1)"
core by Earle Philhower as well as the "Arduino MBED OS RP2040" core (version 4.1.5).
Both cores can be installed in the Arduino IDE via "Tools->Boards Manager".

## Wiring

For 5V platforms such as the Arduino Uno, the IEC bus signals (ATN, Clock, Data, Reset) can be directly 
connected to the microcontroller. The pins can be freely chosen and are configured in the class 
constructor (see [class reference](#iecdevice-class-reference) below). It is recommended to choose an interrupt-capable pin for the ATN 
signal (see [timing considerations](#timing-considerations) section below). 

When looking at the IEC bus connector at the back of your computer, the pins are as follows:<br>
(1=SRQ [not used], 2=GND, 3=ATN, 4=Clock, 5=Data, 6=Reset)

<img src="IECBusPins.jpg" width="25%" align="center">

Here is a picture of an Arduino UNO directly connected to the IEC bus of a C64:

<table>
<tr><td><img src="hardware/pictures/UnoBasicSerial2.jpg" width="100%"></td><td><img src="hardware/pictures/SerialCable.jpg"></td></tr>
</table>
  
For 3.3V platforms (Raspberry Pi Pico, ESP32, Arduino Due) a level shifter is required to isolate the
microcontroller from the 5V signals on the IEC bus. I am using this [SparkFun level converter](https://www.sparkfun.com/products/12009) 
but other models should do just fine as the IEC bus is not particularly fast. Connect the IEC bus signals
and 5V supply to the "High Voltage" side and microcontroller pins and 3.3V supply to the "Low Voltage" side.

For a Raspberry Pi Pico there is [a development board](hardware/README.md#raspberry-pi-pico-development-board)
to simplifiy the level converter wiring:
<img src="hardware/pictures/PiPicoSD.jpg" width="50%" align="center">

## Implementing a simple low-level device

Implementing a basic device using the IECDevice class requires two steps:
  1. Derive a new class from the IECDevice class and implement the device's behavior in the new class
  2. Call the IECDevice::begin() and IECDevice::task() functions within your main sketch functions.

This section describes those steps based on the [IECBasicSerial](examples/IECBasicSerial/) 
example, a simple device that connects a serial (RS232) port to the IEC bus.

First we define a new class, derived from the IECDevice class. 

```
#include <IECDevice.h>

class IECBasicSerial : public IECDevice
{
 public:
  IECBasicSerial();

  virtual int8_t canRead(byte devnum);
  virtual byte   read(byte devnum);

  virtual int8_t canWrite(byte devnum);
  virtual void   write(byte devnum, byte data, bool eoi);
};
```

We implement the device functions by overriding the canRead/read/canWrite/write functions.
See the [IECDevice Class Reference](#iecdevice-class-reference) section below for a detailed description of these functions:

```
IECBasicSerial::IECBasicSerial() : IECDevice(3, 4, 5)
{}
```
The class constructor must call the IECDevice() constructor which defines the microcontroller 
pins (ATN=3, Clock=4, Data=5) to which the IEC bus signals are connected.

```
int8_t IECBasicSerial::canRead(byte devnum) {
  byte n = Serial.available();
  return n>1 ? 2 : n;
}
```
The canRead(devnum) function is called whenever data is requested from the device. For this device
we return 0 if we have nothing to send. This will cause a timeout error condition on the bus
(there is no provision in the IEC bus protocol for the computer to ask a device whether it
has data to send at all). On the computer side this will set bit 1 of the status word (i.e.
the ST variable in BASIC). If we returned -1 then canRead() would be called repeatedly, 
blocking the bus until we have something to send. That would prevent us from receiving incoming 
data on the bus.

```
byte IECBasicSerial::read(byte devnum) { 
  return Serial.read();
}
```
The read() function will **only** be called if the previous call to canRead() returned a value greater than 0.
Since canRead() returned non-zero we know that serial data is availabe so we can just return the
result of Serial.read()

```
int8_t IECBasicSerial::canWrite(byte devnum) {
  return Serial.availableForWrite()>0 ? 1 : -1;
}
```
The canWrite() function will be called whenever the computer wants to send data to the device.
We return -1 if the serial port can not accept data (the serial transmit buffer is full).
This will cause canWrite() to be called again until we are ready and return 1. 
Alternatively we could just wait within this function until we are ready.

```
void IECBasicSerial::write(byte devnum, byte data, bool eoi) { 
  Serial.write(data);
}
```
The write() function will **only** be called if the previous call to canWrite() returned 1. 
So at this point we know already that the serial port can accept data and just pass it on.

To implement our device class in a sketch we must instantiate the class and call the "begin()" and "task()"
functions:

```
IECBasicSerial iecSerial;

void setup()
{
  Serial.begin(115200);
  iecSerial.begin(6);
}

void loop()
{
  iecSerial.task();
}
```

begin() must be called once to set the device number and initialize the IECDevice object and task()
must be called repeatedly as it handles the bus communication and calls our canRead/read/canWrite/write
functions when necessary.  See the [IECDevice Class Reference](#iecdevice-class-reference) section for a detailed
description of these functions.

To interact with this device in BASIC, use the following program:
```
10 OPEN 1,4
20 GET#1,A$:IF (ST AND 2)=0 THEN PRINT A$;
30 GET A$:IF A$<>"" THEN PRINT#1, A$;
40 GOTO 20
```
Any characters typed on the computer's keyboard will be sent out on the microcontroller's serial
connection (at 115200 baud) and incoming serial data will be shown on the computer's screen.

## Implementing a simple file-based device

Implementing a file-based device using the IECFileDevice class requires two steps:
  1. Derive a new class from the IECFileDevice class and implement the device's behavior in the new class
  2. Call the IECFileDevice::begin() and IECFileDevice::task() functions within your main sketch functions.

This section describes those steps based on the [IECBasicSD](examples/IECBasicSD/) 
example, a very simple device to read/write SD cards.
Note that this device will be limited in its functionality, it allows loading and saving programs
on the SD card but no other functionality (directory listing, status channel, deleting files etc..).
The purpose of this section is to demonstrate basic bus communication for file-based devices using the
IECFileDevice class. A more feature-complete implementation of a SD card reader is provided in
the [IECSD example](examples/IECSD). 

Note that any device derived from the IECFileDevice class automatically supports the [JiffyDos](#jiffydos-support),
[Epyx FastLoad](#epyx-fastload-support) and [DolphinDos](#dolphindos-support) protocol.

First, a new class is defined and derived from the IECFileDevice class. 

```
#include <IECFileDevice.h>
#include <SdFat.h>

class IECBasicSD : public IECFileDevice
{
 public: 
  IECBasicSD();

 protected:
  virtual void open(byte devnum, byte channel, const char *name);
  virtual byte read(byte devnum, byte channel, byte *buffer, byte bufferSize);
  virtual byte write(byte devnum, byte channel, byte *buffer, byte n);
  virtual void close(byte devnum, byte channel);

 private:
  SdFat  m_sd;
  SdFile m_file;
};
```

We implement the device functions by overriding the open/read/write/close functions.
See the [IECFileDevice Class Reference](#iecfiledevice-class-reference) section below for a detailed description of these functions:

```
IECBasicSD::IECBasicSD() : IECFileDevice(3, 4, 5, 6)
{
  m_sd.begin(10);
}
```

The class constructor must call the IECFileDevice() constructor which defines the microcontroller
pins to which the IEC bus signals are connected (ATN=3, Clock=4, Data=5, Reset=6). 
We also initialize the SD card interface in the constructor, using pin 10 for CS.

```
void IECBasicSD::open(byte devnum, byte channel, const char *name)
{
  m_file.open(name, channel==0 ? O_RDONLY : (O_WRONLY | O_CREAT));
}
```

The "open()" function is called whenever the bus controller (computer) issues an OPEN command.
Note that this function does not return a value to signify success or failure to open the
file. The IEC bus protocol does not provide a method to transmit this information directly.
For more information on this see the description of the open() function in 
[IECFileDevice Class Reference](#iecfiledevice-class-reference) section below.

```
byte IECBasicSD::read(byte devnum, byte channel, byte *buffer, byte bufferSize)
{
  return m_file.isOpen() ? m_file.read(buffer, bufferSize) : 0;
}
```

This function must fill the given buffer with up to bufferSize bytes of data from
the file that was previously opened for the given channel number. It must return the number of bytes 
written to the buffer. Returning 0 signals that no more data is left to read. 
Returning 0 on the first call after "open()" signals that there was an
error opening the file.

```
byte IECBasicSD::write(byte devnum, byte channel, byte *buffer, byte n)
{
  return m_file.isOpen() ? m_file.write(buffer, n) : 0;
}
```

This function must write *n* data bytes from the buffer to the file that was previously opened 
for the given channel number and return the number of bytes written. Returning a number less 
than n signals an error condition.

```
void IECBasicSD::close(byte devnum, byte channel)
{
  m_file.close(); 
}
```

This function is called when the bus controller (computer) sends a CLOSE command.
It should close the data file previously opened for the given channel. 

To implement our device class in a sketch we must instantiate the class and call the "begin()" and "task()"
functions:

```
IECBasicSD iecSD;

void setup()
{
  iecSD.begin(9);
}

void loop()
{
  iecSD.task();
}
```

begin() must be called once to set the device number and initialize the IECFileDevice object and task()
must be called repeatedly as it handles the bus communication and calls our canRead/read/canWrite/write
functions when necessary.  See the [IECFileDevice Class Reference](#iecfiledevice-class-reference) 
section for a detailed description of these functions.

## IECDevice class reference

The IECDevice class has the following functions that may/must be called from your code:

- ```IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF)```  
  The IECDevice constructor defines the pins to which the IEC bus signals care connected and must be called from
  the constructor of your derived class. The pinRESET parameter is optional. 
  If not given, the device will simply not respond to a bus reset. The pinCTRL parameter (also optional) is helpful
  for applications where the microcontroller may not be able to respond quickly enough to ATN requests 
  (see [Timing considerations](#timing-considerations) section below).

- ```void begin(byte devnum)```  
  This function must be called once at startup before the first call to "task", devnum
  is the IEC bus device number that the device should react to. begin() may be called
  again later to switch to a different device number.

- ```void addDeviceNumber(byte devnum)```  
  The call to "begin" already defines the device number for your device. If the device should
  answer to multiple different device numbers, call "addDeviceNumber" to add more numbers.
  The "devnum" argument passed to the communication functions below (canRead/read/canWrite/write)
  will indicate the device number that was addressed.
  
  **NOTE:** You must increase the ```#define MAX_DEVICES``` in file IECDevice.h if you want to call
  this function. By default, MAX_DEVICES is set to 1 in order to save memory on smaller platforms.
  Calling addDeviceNumber() will have no effect if MAX_DEVICES is set to 1.

- ```void task()```
  This function must be called periodically to handle IEC bus communication
  if the ATN signal is NOT connected to an interrupt-capable pin on your microcontroller
  then task() must be called at least once every millisecond. Otherwise you may get "Device not present"
  errors when trying to communicate with your device. If ATN is on an interrupt-capable
  pin less frequent calls are ok but bus communication will be slower if called less frequently.

- ```void enableJiffyDosSupport(bool enable)```  
  This function must be called **if** your device should support the JiffyDos protocol.
  In most cases devices with JiffyDos support should be derived from the IECFileDevice class
  which handles JiffyDos support internally and you do not have to call enableJiffyDosSupport().
  For more information see the [JiffyDos support](jiffydos-support) section below.
  You can also use this function to disable JiffyDos support after it has been enabled.

- ```void enableEpyxFastLoadSupport(bool enable)```  
  This function must be called **if** your device should support the Epyx FastLoad protocol.
  In most cases devices with Epyx FastLoad support should be derived from the IECFileDevice class
  which handles Epyx FastLoad support internally and you do not have to call enableEpyxFastLoadSupport().
  For more information see the [Epyx FastLoad support](epyx-fastload-support) section below.
  You can also use this function to disable Epyx FastLoad support after it has been enabled.

- ```void enableDolphinDosSupport(bool enable)```  
  This function must be called **if** your device should support the DolphinDos parallel protocol.
  In most cases devices with DolphinDos support should be derived from the IECFileDevice class
  which handles DolphinDos support internally and you do not have to call enableDolphinDosSupport().
  For more information see the [DolphinDos support](#dolphindos-support) section below.
  You can also use this function to disable DolphinDos support after it has been enabled.

- ```void setBuffer(byte *buffer, byte bufferSize);```
  This function shold be called **before** calling enableJiffyDosSupport(), enableDolphinDosSupport()
  or enableEpyxFastloadSupport()
  to set the required data buffer for data block transmissions. The IECFileDevice class calls this
  function, so if your device class is derived from IECFileDevice you do not need to call setBuffer().
  If not called, a default buffer of size 1 will be used which is very inefficient (but saves memory).
  A good buffer size is 128.

- ```void setDolphinDosPins(...)```
  This function can be called **before** calling enableDolphinDosSupport to specify the pins to be used
  for the DolphinDos parallel cable. See the [DolphinDos support](#dolphindos-support) section below 
  for more information.

The following functions can be overloaded in the derived device class to implement the device functions.
None of these function are *required*. For example, if your device only receives data then only the
canWrite() and write() functions need to be overloaded.

The devnum argument passed to these functions indicates the device number that the host has addressed.
Usually this is the device number passed into the "begin()" call (see above). However if more device
numbers were added via the "addDeviceNumber" function then devnum may show any of those numbers.
  
- ```int8_t canRead(byte devnum)```  
  This function will be called whenever the device is asked to send data
  to the bus controller (i.e. the computer). It should return one of four values: -1, 0, 1 or 2.  
  Returning -1 signals that we do not know yet whether there is more data to send.
  The canRead() function will be called again later and until then the bus will remain blocked.
  Alternatively, the canRead() function may wait on its own until an answer is known.  
  Returning 0 signals that there is no data to read.  
  Returning 1 signals that there is **exactly** one byte of data left to read.  
  Returning 2 signals that there are two or more bytes of data left to read.
- ```byte read(byte devnum)```  
  This function is called **only** if the previous call to canRead() returned a value greater than 0.
  read() must return the next data byte.
- ```int8_t canWrite(byte devnum)```  
  This function will be called whenever the bus controller (computer) sends data
  to your device. It should return one of three values: -1, 0 or 1.   
  Returning -1 signals that we do not know yet whether we can accept more data.
  The canWrite() function will be called again later and until then the bus will remain blocked.
  Alternatively, the canWrite() function may wait on its own until an answer is known.  
  Returning 0 signals that we are not able to accept more data.  
  Returning 1 signals that we can accept data.  
  canWrite() should **only** return 1 if the device is ready to receive and process the data immediately.
- ```void write(byte devnum, byte data, bool eoi)``` 
  This function is called **only** if the previous call to canWrite() returned 1. The data argument
  is the data byte received on the bus. Note that the write() function must process the data and return 
  immediately (within 1 millisecond), otherwise bus timing errors may occur. 
  The eoi argument will be "true" if the host indicated that this is the last byte of a trasmission.
- ```void listen(byte devnum, byte secondary)```  
  Called when the bus controller (computer) issues a LISTEN command, i.e. is about to send data to the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void unlisten(byte devnum)```  
  Called when the bus controller (computer) issues an UNLISTEN command, i.e. is done sending data.
- ```void talk(byte devnum, byte secondary) ```  
  Called when the bus controller (computer) issues a TALK command, i.e. is requesting data from the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void untalk(byte devnum)```  
  Called when the bus controller (computer) issues an UNTALK command, i.e. is done receiving data from the device.
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).



The following functions should be overloaded if the JiffyDos protocol should be supported.
In most cases devices with JiffyDos support should be derived from the IECFileDevice class
which handles JiffyDos support internally and you do not have to implement these functions.
For more information see the [JiffyDos support](jiffydos-support) section below.

- ```byte peek(byte devnum)```  
  Called when the device is sending data using JiffyDos byte-by-byte protocol.  
  peek() will only be called if the last call to canRead() returned >0.  
  peek() should return the next character that will be read with read().  
  peek() is allowed to take an indefinite amount of time.  
- ```byte read(byte devnum, byte *buffer, byte bufferSize)```  
  This function is called when the device is sending data using the JiffyDos block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, JiffyDos load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  

The following function should be overloaded if the Epyx FastLoad protocol should be supported.
In most cases devices with Epyx FastLoad support should be derived from the IECFileDevice class
which handles Epyx FastLoad support internally and you do not have to implement these functions.
For more information see the [Epyx FastLoad support](epyx-fastload-support) section below.

- ```byte read(byte devnum, byte *buffer, byte bufferSize)```  
  This function is called when the device is sending data using the Epyx FastLoad block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, Epyx FastLoad load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  


The following functions should be overloaded if the DolphinDos parallel protocol should be supported.
In most cases devices with DolphinDos support should be derived from the IECFileDevice class
which handles DolphinDos support internally and you do not have to implement these functions.
For more information see the [DolphinDos support](#dolphindos-support) section below.

- ```byte write(byte devnr, byte *buffer, byte bufferSize, bool eoi)```
  This function is only called when the device is receiving data using the DolphinDos block transfer (SAVE protocol).
  write() should process all the data in the buffer and return the number of bytes processed.
  Returning a number lower than bufferSize signals an error condition.
  The "eoi" parameter will be "true" if sender signaled that this is the final part of the transmission
  If write() is **not** overloaded, DolphinDos save performance will be several times slower than otherwise.
  write() is allowed to take an indefinite amount of time.
- ```byte read(byte devnum, byte *buffer, byte bufferSize)```  
  This function is only called when the device is sending data using the DolphinDos block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, DolphinDos load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  
- ```void enableDolphinBurstMode(bool enable)```, ```void dolphinBurstReceiveRequest()```, ```void dolphinBurstTransmitRequest()```
  In DolphinDos, the burst (fast) transfer mode is controlled by commands sent via the command
  channel (channel 15). During a LOAD/SAVE operation, the host will send "XQ"/"XZ" on the command channel
  which then should cause the device to confirm the burst transmission. Since the low-level IECDevice
  class itself does not handle the command channel, it provides functions for a higher-level class
  to signal the burst request: Call dolphinBurstTransmitRequest() if "XQ" is received on the command channel.
  Call dolphinBurstReceiveRequest() if "XZ" is received on the command channel. You can also call 
  enableDolphinBurstMode() to enable/disable support of burst transfers ("XF+"/"XF-" DolphinDos command).

## IECFileDevice class reference

The IECFileDevice class has the following functions that may/must be called from your code:

- ```IECFile  Device(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF)```  
  The IECDevice constructor defines the pins to which the IEC bus signals care connected and must be called from
  the constructor of your derived class. The pinRESET parameter is optional. 
  If not given, the device will simply not respond to a bus reset. The pinCTRL parameter (also optional) is helpful
  for applications where the microcontroller may not be able to respond quickly enough to ATN requests 
  (see [Timing Considerations](#timing-considerations) section below).

- ```void begin(byte devnum)```  
  This function must be called once at startup before the first call to "task", devnum
  is the IEC bus device number that the device should react to. begin() may be called
  again later to switch to a different device number.

- ```void addDeviceNumber(byte devnum)```  
  The call to "begin" already defines the device number for your device. If the device should
  answer to multiple different device numbers, call "addDeviceNumber" to add more numbers.
  The "devnum" argument passed to the communication functions below (canRead/read/canWrite/write)
  will indicate the device number that was addressed.

  **NOTE:** You must increase the ```#define MAX_DEVICES``` in file IECDevice.h if you want to call
  this function. By default, MAX_DEVICES is set to 1 in order to save memory on smaller platforms.
  Calling addDeviceNumber() will have no effect if MAX_DEVICES is set to 1.

- ```void setDolphinDosPins(...)```
  This function can be called before calling enableDolphinDosSupport to specify the pins to be used
  for the DolphinDos parallel cable. See the [DolphinDos support](#dolphindos-support) section below 
  for more information.

- ```void task()```
  This function must be called periodically to handle IEC bus communication
  if the ATN signal is NOT connected to an interrupt-capable pin on your microcontroller
  then task() must be called at least once every millisecond. Otherwise you may get "Device not present"
  errors when trying to communicate with your device. If ATN is on an interrupt-capable
  pin less frequent calls are ok but bus communication will be slower if called less frequently.

The following functions can be overloaded in the derived device class to implement the device functions.
None of these function are *required*. For example, if your device only receives data then only the
canWrite() and write() functions need to be overloaded.

Most of these functions take an argument named *channel* which is the channel number given in
the OPEN command that opened the file on the computer side, i.e. ```OPEN fileNum, deviceNum, channel, name$```
The channel number will be in the range 0 to 14.  
Channel 15 is the status and command channel. If the computer reads from channel 15 on your device, 
the getStatus() function will be called and the result of that call will be sent to the computer.
If the computer writes to channel 15 on your device, the execute() function will be called that lets
the device process the command.

The device number (devnum) passed to these functions indicates the device number that the host has addressed.
Usually this is the device number passed into the "begin()" call (see above). However if more device
numbers were added via the "addDeviceNumber" function then devnum may show any of those numbers.
  
The channel number will be 0 when the computer executes a LOAD command and 1 when the computer 
executes a SAVE command.
  
- ```void open(byte devnum, byte channel, const char *filename)```  
  This function is called whenever the bus controller (computer) issues an OPEN command.
  The *channel* parameter specifies the channel as described above and the *filename* 
  parameter is a zero-terminated string representing the file name given in the OPEN command.

  Note that open() does not return a value to signify success or failure to open the
  file. The IEC bus protocol does not provide a method to transmit this information directly.
  
  On the computer side, success or failure for opening a file can be determined by attempting to 
  read/write to it and checking the bus status (ST variable in BASIC) afterwards. If the read/write
  failed (ST<>0) then the file could not be opened.
  
  When LOADing a program, the computer will display a "file not found" error if the device returns 0
  on the *first* read() call  after open().
  
  For SAVEing a program, the computer will never show an error even in the case of failure. It is
  expected that the device signals the error condition separately to the user (e.g. the blinking
  LED on a floppy disk drive).
- ```void close(byte devnum, byte channel)```  
  Close the file that was previously opened on *channel*. The close() function does not have a 
  return value to signal success or failure since the IEC bus protocol does not include a method 
  to transmit this information.  
- ```byte read(byte devnum, byte channel, byte *buffer, byte bufferSize)```  
  Read up to *bufferSize* bytes of data from the file opened for *channel*, returning the number 
  of bytes read. Returning 0 will signal end-of-file to the receiver. Returning 0
  for the FIRST call after open() signals an error condition.
  (LOAD on the computer will show "file not found" in this case)
- ```byte write(byte devnum, byte channel, byte *buffer, byte bufferSize)```  
  Write *bufferSize* bytes of data to the file opened for *channel*, returning the number 
  of bytes written. Returning a number less than *bufferSize* signals an error condition.
- ```void getStatus(byte devnum, char *buffer, byte bufferSize)```  
  Called when the computer reads from channel 15 and the status
  buffer is currently empty. This should populate *buffer* with an appropriate, zero-terminated
  status message of length up to *bufferSize*.
- ```void execute(byte devnum, const char *command, byte cmdLen)```  
  Called when the computers sends data (i.e. a command) to channel 15.
  The *command* parameter is a 0-terminated string representing the command to execute,
  *commandLen* gives the full length of the received command which can be useful if
  the command itself may contain zeros.
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).

## Timing considerations

The IECDevice library deals with most bus timing requirements internally so you don't have to.
However, some need to be considered - especially for lower-powered devices such as the Arduino.

The IEC bus protocol requires that all devices on the bus react to an ATN request
(ATN signal going high->low) by pulling the DATA line low **within 1 millisecond**.
If this does not happen the computer may report "DEVICE NOT PRESENT" errors when
addressing your device.

A device implemented using this library has three ways of doing so:
1. Use an interrupt-capable pin to connect the ATN signal and make sure to never
   disable interrupts for more than one millisecond. On the Raspberry Pi Pico
   and ESP32 all pins are interrupt-capable but for the Arduino devices only
   some are (e.g. on the Arduino Uno only pins 2 and 3).
2. Make sure to call the IECDevice::task() function at least once every millisecond.
   If you can't use an interrupt-capable pin (maybe those are already used for other
   functions) then make sure to call the task() function often enough and the library
   will deal with it.
3. Get a little help from extra hardware. If you can not guarantee the 1ms response
   time in software then a small circuit added to your device can help. This is in fact
   the way that the C1541 floppy drive handles this requirement (albeit using a slightly
   different circuit). Add a 74LS125 buffer to your design and connect it up like this:  
   <img src="hardware/ATNCircuit.png" width="50%">  
   Connect the ATN and Data signals to the bus and the CTRL signal to any available
   pin on your microcontroller. Then in your sketch make sure to add the CTRL pin
   when calling the IECDevice constructor. The purpose of the circuit is to pull
   DATA low immediately when ATN goes low and only release it once the software
   confirms (via the CTRL pin) that it now is in control of the Data signal.

Apart from the ATN signal timing requirements there are a few functions in the 
IECDevice class that have limitations on how long they may take before returning.
Those are described in the [IECDevice Class Reference](#iecdevice-class-reference) section.

Devices derived from the IECFileDevice class have no requirements apart from the ATN timing
as the IECFileDevice class handles all of them internally.

Finally, JiffyDos and Epyx FastLoad transfers require very precise timing which requires the IECDevice
library to disable all interrupts during such transfers. So be aware that if JiffyDos or Epyx FastLoad
support is enabled, the IECDevice::task() function may take up to to 20ms before returning and
with interrupts disabled during JiffyDos transfers. Devices derived from the IECFileDevice
class will automatically have JiffyDos and Epyx FastLoad support enabled. See the 
[JiffyDos support](jiffydos-support) and [Epyx FastLoad support](expyx-fastload-support) sections 
below for how to disable JiffyDos support if desired.

## JiffyDos support

The IECDevice class includes support for the [JiffyDos](https://www.go4retro.com/products/jiffydos/) 
bus protocol which significantly speeds up bus transfers, especially LOAD commands. 
The library automatically detects when the computer requests a JiffyDos transfer and responds correspondingly.

For high-level file-based devices (derived from the IECFileDevice class), all functionality
for JiffyDos support is already included in the IECFileDevice class. JiffyDos support is 
automatically enabled. In case you do NOT want your device to support JiffyDos, just add
the following call in the body of your class constructor: ```enableJiffyDosSupport(false)```

To completely disable JiffyDos support (for example to save memory space on small controllers
like the Arduino UNO), comment out the "#define SUPPORT_JIFFY" line at the top of file IECDevice.h

For low-level devices (derived from the IECDevice class), two additional functions need to 
be overloaded: ```peek()``` must return the next data byte that will be retuned by a call
to ```read()``` and ```read(buffer, bufferSize)``` which when called should return 
a chunk of data to be transferred. See the [IECDevice class reference](#iecdevice-class-reference) section
for the full function definitions.

Even with these functions overloaded, JiffyDos support is initially disabled for low-level devices
and must be enabled by calling ```setBuffer(buffer, bufferSize)``` and ```enableJiffyDosSupport(true)```
in you class constructor.

The ```setBuffer``` function defines a buffer which the IECDevice needs to store data during JiffyDos
transfers. The size of the given buffer affects performance but sizes above 128 bytes do not
increase performance much above 128 bytes which is what I would recommend unless memory is
not an issue (maximum buffer size is 255 bytes).

As mentioned in the timing consideration section, interrupts will be disabled during JiffyDos transfers 
which may cause your program to not be able to respond to interrupts for up to 20ms at a time.

Note that in order to use the fast JiffyDos routins you need a C64 replacement kernal that includes 
the JiffyDos transfer routines which can be purchased [here](https://store.go4retro.com/categories/Commodore/Firmware/JiffyDOS).

## Epyx FastLoad support

The IECDevice class includes support for the [Epyx FastLoad](https://en.wikipedia.org/wiki/Epyx_Fast_Load)
bus protocol which significantly speeds up LOAD commands. 
The library automatically detects when the computer requests an Epyx FastLoad transfer and responds correspondingly.

For high-level file-based devices (derived from the IECFileDevice class), all functionality
for Epyx FastLoad support is already included in the IECFileDevice class. Epyx FastLoad support is 
automatically enabled. In case you do NOT want your device to support FastLoad, just add
the following call in the body of your class constructor: ```enableEpyxFastLoadSupport(false)```.

To completely disable FastLoad support (for example to save memory space on small controllers
like the Arduino UNO), comment out the "#define SUPPORT_EPYX" line at the top of file IECDevice.h

For low-level devices (derived from the IECDevice class), an additional functions need to 
be overloaded: ```read(buffer, bufferSize)``` should return a chunk of data to be transferred.
See the [IECDevice class reference](#iecdevice-class-reference) section for the full function definition.

Even with this function overloaded, FastLoad support is initially disabled for low-level devices
and must be enabled by calling ```setBuffer(buffer, bufferSize)``` and ```enableEpyxFastLoadSupport(true)```
in you class constructor. The buffer must have a size of at least 32 bytes.

The ```setBuffer``` function defines a buffer which the IECDevice needs to store data during FastLoad
transfers. The size of the given buffer affects performance but sizes above 128 bytes do not
increase performance much above 128 bytes which is what I would recommend unless memory is
not an issue (maximum buffer size is 255 bytes).

As mentioned in the timing consideration section, interrupts will be disabled during FastLoad transfers 
which may cause your program to not be able to respond to interrupts for up to 20ms at a time.

Note that in order to use the FastLoad routins you need the Epyx FastLoad cartridge for your C64
(https://en.wikipedia.org/wiki/Epyx_Fast_Load). A modern replica of the cartridge can be purchased
at [TFW8B](https://www.tfw8b.com/product/epyx-fastload-reloaded-disk-sd2iec-turbo-loader-cartridge-c64).

## DolphinDos support

**Note:** Since DolphinDos support requires a number of additional pins for the parallel connection 
and DolphinDos is not a very widely used fast loader, it not enabled by default. To enable DolphinDos
support, un-comment the ```#define SUPPORT_DOLPHIN``` line at the top of file src/IECDevice.h.

The IECDevice class includes support for the [DolphinDos](https://rr.pokefinder.org/wiki/Dolphin_DOS)
parallel protocol.

I recommend deriving any device class with DolphinDos support from the higher-level file-based
IECFileDevice class. It is possible to use the lower-level IECDevice class (with steps similar
to those described in the JiffyDos) section above but DolphinDos requires additional steps for
handling burst transfer requests (XQ and XZ) on the command channel. If you really want to 
develop your own low-level DolphinDos class, search for "dolphin" in the IECFileDevice.cpp file
to see what additional steps are taken there.

Like JiffyDos, DolphinDos needs a replacement kernal in the C64 for its fast transmission routines.
The DolphinDos V2 C64 kernal can be downloaded [here](https://e4aws.silverdr.com/projects/dolphindos2/).

DolphinDos relies on a parallel connection between the computer and device for its improved
transmission speed. It will work without parallel cable will not provide any speed improvement.

IECDevice has pre-defined pins for the different hardware platforms but those can be changed
by calling the ```setDolphinDosPins(HT,HR,D0,D1,D2,D3,D4,D5,D6,D7)``` function. The pre-defined
pin numbers are as follows:

C64 User Port pin | Signal | Arduino Uno | Mega | Due | Raspberry Pi Pico | ESP32
------------------|--------|-------------|------|-----|-------------------|------
C (PB0)           | D0     | A0          | 22   | 51  | 7                 | IO13
D (PB1)           | D1     | A1          | 23   | 50  | 8                 | IO14   
E (PB2)           | D2     | A2          | 24   | 49  | 9                 | IO15   
F (PB3)           | D3     | A3          | 25   | 48  | 10                | IO16   
H (PB4)           | D4     | A4          | 26   | 47  | 11                | IO17   
J (PB5)           | D5     | A5          | 27   | 46  | 12                | IO25   
K (PB6)           | D6     | 8           | 28   | 45  | 13                | IO26   
L (PB7)           | D7     | 9           | 29   | 44  | 14                | IO27   
8 (PC2)           | HR     | 2           | 2    | 53  | 15                | IO36 (VP)  
B (FLAG2)         | HT     | 7           | 30   | 52  | 6                 | IO4    

Instructions for making a breakout board for the C64 user port that allows for easy connection 
to the parallel port pins are available [here](hardware/README.md#user-port-breakout-board)

## Raspberry Pi Pico development board

Instructions for a development board to connect a Raspberry Pi Pico to a C64 are available 
[here](hardware/README.md#raspberry-pi-pico-development-board)

<table>
<tr><td><center>Connected using serial IEC cable only</center></td><td><center>Connected using serial IEC and parallel cable</center></td></tr>
<tr><td><img src="hardware/pictures/PiPicoSD.jpg"></td><td><img src="hardware/pictures/PiPicoSDParallel.jpg"></td></tr>
</table>
