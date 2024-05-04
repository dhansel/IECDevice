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

## Installation

To install this library, click the green "Code" button on the top of this page and select "Download ZIP".
Then in the Arduino IDE select "Sketch->Include Library->Add ZIP Library" and select the downloaded ZIP file.
After doing so you will find the included examples in File->Examples->IECDevice->...".

## Wiring

For 5V platforms such as the Arduino Uno, the IEC bus signals (ATN, Clock, Data, Reset) can be directly 
connected to the Microcontroller. The pins can be freely chosen and are configured in the class 
constructor (see description below). It is recommended to choose an interrupt-capable pin for the ATN 
signal. 

For 3.3V platforms (Raspberry Pi Pico, ESP32, Arduino Due) a level shifter is required to isolate the
controller from the 5V signals on the IEC bus. I am using this [SparkFun level converter](https://www.sparkfun.com/products/12009) 
other models should do fine as the IEC bus is not particularly fast. Connect the IEC bus signals
plus 5V supply to the "High Voltage" side and microcontroller pins plus 3.3V supply to the "Low Voltage" side.

## Implementing a simple low-level device

Implementing a basic device using the IECDevice class requires two steps:
  1. Derive a new class from the IECDevice class and implement the device's behavior in the new class
  2. Call the IECDevice::begin() and IECDevice::task() functions within your main sketch functions.

This section describes those steps based on the [IECBasicSerial](examples/IECBasicSerial/IECBasicSerial.ino) 
example, a simple device that connects a serial (RS232) port to the IEC bus.

First, a new class is defined and derived from the IECDevice class. 

```
#include <IECDevice.h>

class IECBasicSerial : public IECDevice
{
 public:
  IECBasicSerial();

  virtual int8_t canRead();
  virtual byte   read();

  virtual int8_t canWrite();
  virtual void   write(byte data);
};
```

We implement the device functions by overriding the canRead/read/canWrite/write functions.
See the "IECDevice class reference" section below for a detailed description of these functions:

```
IECBasicSerial::IECBasicSerial() : IECDevice(3, 4, 5)
{}
```
The class constructor must call the IECDevice() constructor which defines the pins (ATN=3, Clock=4, Data=5)
to which the IEC bus signals are connected.

```
int8_t IECBasicSerial::canRead() {
  byte n = Serial.available();
  return n>1 ? 2 : n;
}
```
The canRead() function is called whenever data is requested from the device. For this device
we return 0 if we have nothing to send which indicates a "nothing to send" (error) condition 
on the bus. If we returned -1 instead then canRead() would be called repeatedly, blocking the 
bus until we have something to send. That would prevent us from receiving incoming data on the bus.

```
byte IECBasicSerial::read() { 
  return Serial.read();
}
```
The read() function will **only** be called if the previous call to canRead() returned a value greater than 0.
Since canRead() returned non-zero we know that serial data is availabe so we can just return the
result of Serial.read()

```
int8_t IECBasicSerial::canWrite() {
  return Serial.availableForWrite()>0 ? 1 : -1;
}
```
The canWrite() function will be called whenever the computer wants to send data to the device.
We return -1 if the serial port can not accept data (the serial transmit buffer is full).
This will cause canWrite() to be called again until we are ready and return 1. 
Alternatively we could just wait within this function until we are ready.

```
void IECBasicSerial::write(byte data) { 
  Serial.write(data);
}
```
The write(data) function will **only** be called if the previous call to canWrite() returned 1. 
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

begin() must be called once to set the device number and initialize the device and task()
must be called repeatedly as it handles the bus communication and calls our canRead/read/canWrite/write
functions when necessary.  Again, see the "IECDevice class reference" section for a detailed
description of these functions.

## Implementing a file-based device

Implementing a file-based device using the IECFileDevice class requires two steps:
  1. Derive a new class from the IECFileDevice class and implement the device's behavior in the new class
  2. Call the IECFileDevice::begin() and IECFileDevice::task() functions within your main sketch functions.

This section describes those steps by creating a very simple device to read/write SD cards.
Note that this device will be limited in its functionality, it allows loading and saving programs
on the SD card but no other functionality (directory listing, deleting files etc..).
The purpose of this section is to demonstrate basic bus communication for file-based devices using the
IECFileDevice class. A more feature-complete implementation of a SD card reader is provided in
the [IECSD example](examples/IECSD). 

Note that any device derived from the IECFileDevice class automatically supports the JiffyDos protocol.

First, a new class is defined and derived from the IECFileDevice class. 

```
#include <IECFileDevice.h>
#include <SdFat.h>

class IECBasicSD : public IECFileDevice
{
 public: 
  IECBasicSD();

 protected:
  virtual void open(byte channel, const char *name);
  virtual byte read(byte channel, byte *buffer, byte bufferSize);
  virtual bool write(byte channel, byte data);
  virtual void close(byte channel);

 private:
  SdFat  m_sd;
  SdFile m_file;
};
```

We implement the device functions by overriding the open/read/write/close functions.
See the "IECDevice class reference" section below for a detailed description of these functions:

```
IECBasicSD::IECBasicSD() : IECFileDevice(3, 4, 5)
{
  m_sd.begin(8, SD_SCK_MHZ(1));
}
```

The class constructor must call the IECFileDevice() constructor which defines the pins (ATN=3, Clock=4, Data=5)
to which the IEC bus signals are connected. We also initialize the SD card interface in the constructor.

```
void IECBasicSD::open(byte channel, const char *name)
{
  m_file.open(name, channel==0 ? O_RDONLY : (O_WRONLY | O_CREAT));
}
```


```
byte IECBasicSD::read(byte channel, byte *buffer, byte bufferSize)
{
  return m_file.isOpen() ? m_file.read(buffer, bufferSize) : 0;
}
```

```
bool IECBasicSD::write(byte channel, byte data)
{
  return m_file.isOpen() && m_file.write(&data, 1)==1;
}
```

```
void IECBasicSD::close(byte channel)
{
  m_file.close(); 
}
```

## IECDevice class reference

The IECDevice class has the following functions that may/must be called from your code:

- ```IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF)```  
  The IECDevice constructor defines the pins to which the IEC bus signals care connected and must be called from
  the constructor of your derived class. The pinRESET parameter is optional. 
  If not given, the device will simply not respond to a bus reset. The pinCTRL parameter (also optional) is helpful
  for applications where the microprocessor can not respond fast enough to a ATN request (see "Timing consideration" 
  section below).

- ```void begin(byte devnr)```  
  This function must be called once at startup before the first call to "task", devnr
  is the IEC bus device number that the device should react to. begin() may be called
  again later to switch to a different device number.

- ```void task()```
  This function must be called periodically to handle IEC bus communication
  if the ATN signal is NOT connected to an interrupt-capable pin on your microcontroller
  then task() must be called at least once every millisecond. Otherwise you may get "Device not present"
  errors when trying to communicate with your device. If ATN is on an interrupt-capable
  pin less frequent calls are ok but bus communication will be slower if called less frequently.

- ```void enableJiffyDosSupport(byte *buffer, byte bufferSize)```  
  This function must be called **if** your device should support the JiffyDos protocol.
  In most cases devices with JiffyDos support should be derived from the IECFileDevice class
  which handles JiffyDos support internally and you do not have to call enableJiffyDosSupport().
  For more information see the "JiffyDos support" section below.

The following functions can be overridden in the derived device class to implement the device functions.
None of these function are *required*. For example, if your device only receives data then only the
canWrite() and write() functions need to be overridden.
  
- ```int8_t canRead()```  
  This function will be called whenever the device is asked to send data
  to the bus controller (i.e. the computer). It should return one of four values: -1, 0, 1 or 2.  
  Returning -1 signals that we do not know yet whether there is more data to send.
  The canRead() function will be called again later and until then the bus will remain blocked.
  Alternatively, the canRead() function may wait on its own until an answer is known.  
  Returning 0 signals that there is no data to read.  
  Returning 1 signals that there is **exactly** one byte of data left to read.  
  Returning 2 signals that there are two or more bytes of data left to read.
- ```byte read()```  
  This function is called **only** if the previous call to canRead() returned a value greater than 0.
  read() must return the next data byte.
- ```int8_t canWrite()```  
  This function will be called whenever the bus controller (computer) sends data
  to your device. It should return one of three values: -1, 0 or 1.   
  Returning -1 signals that we do not know yet whether we can accept more data.
  The canWrite() function will be called again later and until then the bus will remain blocked.
  Alternatively, the canWrite() function may wait on its own until an answer is known.  
  Returning 0 signals that we are not able to accept more data.  
  Returning 1 signals that we can accept data.  
  canWrite() should **only** return 1 if the device is ready to receive and process the data immediately.
- ```void write(byte data)``` 
  This function is called **only** if the previous call to canWrite() returned 1. The data argument
  is the data byte received on the bus. Note that the write() function must process the data and return 
  immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void listen(byte secondary)```  
  Called when the bus controller (computer) issues a LISTEN command, i.e. is about to send data to the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void unlisten()```  
  Called when the bus controller (computer) issues an UNLISTEN command, i.e. is done sending data.
- ```void talk(byte secondary) ```  
  Called when the bus controller (computer) issues a TALK command, i.e. is requesting data from the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void untalk()```  
  Called when the bus controller (computer) issues an UNTALK command, i.e. is done receiving data from the device.
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).
- ```byte peek()```  
  Called when the device is sending data using JiffyDOS byte-by-byte protocol.
  peek() will only be called if the last call to canRead() returned >0
  peek() should return the next character that will be read with read()
  peek() is allowed to take an indefinite amount of time.  
  In most cases devices with JiffyDos support should be derived from the IECFileDevice class
  which handles JiffyDos support internally and you do not have to implement peek().
  For more information see the "JiffyDos support" section below.
- ```byte read(byte *buffer, byte bufferSize)```  
  This function is only called when the device is sending data using the JiffyDOS block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is not overloaded, JiffyDOS load performance will be about 3 times slower than otherwise
  read() is allowed to take an indefinite amount of time.  
  In most cases devices with JiffyDos support should be derived from the IECFileDevice class
  which handles JiffyDos support internally and you do not have to implement peek().
  For more information see the "JiffyDos support" section below.

## IECFileDevice class reference

The IECFileDevice class has the following functions that may/must be called from your code:

- ```IECFile  Device(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF)```  
  The IECDevice constructor defines the pins to which the IEC bus signals care connected and must be called from
  the constructor of your derived class. The pinRESET parameter is optional. 
  If not given, the device will simply not respond to a bus reset. The pinCTRL parameter (also optional) is helpful
  for applications where the microprocessor can not respond fast enough to a ATN request (see "Timing consideration" 
  section below).

- ```void begin(byte devnr)```  
  This function must be called once at startup before the first call to "task", devnr
  is the IEC bus device number that the device should react to. begin() may be called
  again later to switch to a different device number.

- ```void task()```
  This function must be called periodically to handle IEC bus communication
  if the ATN signal is NOT connected to an interrupt-capable pin on your microcontroller
  then task() must be called at least once every millisecond. Otherwise you may get "Device not present"
  errors when trying to communicate with your device. If ATN is on an interrupt-capable
  pin less frequent calls are ok but bus communication will be slower if called less frequently.

The following functions can be overridden in the derived device class to implement the device functions.
None of these function are *required*. For example, if your device only receives data then only the
canWrite() and write() functions need to be overridden.
  
- ```void open(byte channel, const char *name)```  
  open file "name" on channel
- ```void close(byte channel)```  
  close file on channel
- ```bool write(byte channel, byte data)```  
  write one byte to file on channel, return "true" if successful
  Returning "false" signals "cannot receive more data" for this file
- ```byte read(byte channel, byte *buffer, byte bufferSize)```  
  read up to bufferSize bytes from file in channel, returning the number of bytes read
  returning 0 will signal end-of-file to the receiver. Returning 0
  for the FIRST call after open() signals an error condition
  (e.g. C64 load command will show "file not found")
- ```void getStatus(char *buffer, byte bufferSize)```  
  called when the bus master reads from channel 15 and the status
  buffer is currently empty. this should populate buffer with an appropriate 
  status message bufferSize is the maximum allowed length of the message
- ```void execute(const char *command, byte cmdLen)```  
  called when the bus master sends data (i.e. a command) to channel 15
  command is a 0-terminated string representing the command to execute
  commandLen contains the full length of the received command (useful if
  the command itself may contain zeros)
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).

## Timing considerations

## JiffyDos support
