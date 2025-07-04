# IECDevice

This library for the Arduino IDE provides a simple interface to connect a variety of current 
microcontrollers to the Commodre IEC bus used on the C64, C128 and VIC-20. This should
make it easier for hobbyists to create new devices for those computers.

The library provides three classes:
  - [```IECDevice```](#iecdevice-class-reference) for creating low-level bus devices that directly respond to the data 
    sent over the bus one byte at a time. This can be used to implement devices such as
    [printers](examples/IECCentronics) or [modem-like](examples/IECBasicSerial) devices.<br>
    For an introduction to creating a device using this class, see the
    [Implementing a simple low-level device](#implementing-a-simple-low-level-device) section.
  - [```IECFileDevice```](#iecfiledevice-class-reference) for creating higher-level devices that operate more like disk
    drives. The IECFileDevice interface is file-based, providing open/close/read/write functions.
    An example use for this class would be an [SD-card reader](examples/IECSD).
    Any device created using this class automatically supports the following fast load protocols:
    - [JiffyDos](#jiffydos-support),
    - [Epyx FastLoad](#epyx-fastload-support)
    - [DolphinDos](#dolphindos-support)
    - [SpeedDos](#speeddos-support)
      
For an introduction to creating a device using this class, see the
    [Implementing a simple file-based device](#implementing-a-simple-file-based-device) section.
  - [```IECBusHandler```](#iecbushandler-class-reference) facilitates the IEC bus communication itself. After creating an instance of the
    IECDevice or IECFileDevice classes it must be attached to the handler by calling ```IECBusHandler::attachDevice()```.<br>
    See the [Implementing a simple low-level device](#implementing-a-simple-low-level-device) 
    section for an example on how to use IECBusHandler class.

So far I have tested this library on the following microcontrollers:
  -  Arduino 8-bit ATMega devices (Uno R3, Mega, Mini, Nano, Micro, Leonardo)
  -  Arduino Uno R4
  -  Arduino Due (32-bit)
  -  ESP32 (in Arduino IDE as well as using PlatformIO with ESP-IDF framework)
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

Generally, devices connected to the IEC bus use line driver ICs (74LS06 or 74LS07) to drive the Clock 
and Data signals. While it is better to do so, it adds some hardware and complexity that 
is not entirely necessary, at least as long as at most one other device (e.g. 1541 disk drive)
is connected to the bus.

Each device on the bus usually has 1k resistors pulling both Clock and Data to +5V. 
That means to drive Clock or Data low, a microcontroller needs to sink n*5mA of current, where "n" 
is the number of other devices on the bus (including the computer).

Different controllers can sink varying amounts of current:
  - The ATMega328p controller (Arduino UNO) can sink about 40mA per pin and a total of 200mA for all pins.
That should be enough for up to 7 other devices.
  - A RP2040 (Raspberry Pi Pico) can sink up to 12mA per pin with a total of 50mA for all pins,
    enough for the computer and one other device (e.g. a 1541).
  - The ESP32 can sink 28mA per pin (total max 250 mA) which should be enough for a computer and up to 4 other devices.

If you want to make sure your microcontroller's I/O pins cannot be overdriven by the sink current, you 
can use a 74LS07 or 74LS06 buffer/line driver. It is certainly the safer way to go, however it adds the 
complication of having to acquire those ICs and wiring them up.

Personally, in all my testing I always connected Clock and Data directly to my IECDevice,
with a C64 and C1541 drive on the bus and never had any problems. This setup is certainly fine 
for playing around and testing. 

### Simplified wiring without using line driver ICs

For 5V platforms such as the Arduino Uno, the IEC bus signals (ATN, Clock, Data, Reset) can be directly 
connected to the microcontroller. The pins can be freely chosen and are configured in the class 
IECBusHandler constructor (see [class reference](#iecbushandler-class-reference) below). 
It is recommended to choose an interrupt-capable pin for the ATN 
signal (see [timing considerations](#timing-considerations) section below). 

When looking at the IEC bus connector at the back of your computer, the pins are as follows:<br>
(1=SRQ, 2=GND, 3=ATN, 4=Clock, 5=Data, 6=Reset)

<img src="IECBusPins.jpg" width="25%" align="center">

Note that the SRQ line will rarely be needed (and can be left unconnected). The SRQ signal allows a device on the
IEC bus to request attention from the computer. In the C64, the SRQ signal is connected to the
FLAG input of CIA1 and therefore, if the CIA is set up to do so, can create a processor interrupt. The
processor can see that the interrupt originated from the SRQ line, i.e. that a device requested attention 
(although it can not see *which* device produced the request) and act accordingly.
This can be useful in some cases, for example if a modem device on the IEC bus wants to signal that more
data is available for reading. However, the C64 kernal itself does not use this signal at all and very
little (if any) software exists that uses it. In general you can leave this pin unconnected, unless you
are building a device that uses it (and you are writing the C64 software for it). If you do want to use
the SRQ functionality, do the following:
  - Pick a pin on your microcontroller for the SRQ signal and connect it to the SRQ line on the IEC bus
  - In the IECBusHandler() constructor, specify the pin used for SRQ
  - When your device needs to cause a SRQ interrupt on the computer, call the IECDevice::sendSRQ() function.

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

### Wiring with line driver ICs

If want to use line drivers, first follow the wiring instructions above. I recommend experimenting with
the simplified wiring first to make sure everything is working. Then
  1. Un-comment the line ```#define USE_LINE_DRIVERS``` in IECConfig.h and re-upload the sketch. If you are using an
     interting line drive (74LS06) then also un-comment ```#define USE_INVERTED_DRIVERS```.
  2. In your code, where you instantiate the IECBusHandler class add two pins for controlling
     the line drivers for the CLK and DATA line to the IECBusHandler constructor call, i.e. change
     ```IECBusHandler myBus(pinATN, pinCLK, pinDATA, pinRESET);``` to
     ```IECBusHandler myBus(pinATN, pinCLK, pinCLKout, pinDATA, pinDATAout, pinRESET);```
  3. Add a 74LS06 or 74LS07 IC to your project and wire it up as follows:
     - Connect the ```pinCLKout``` pin from the constuctor above to an input pin (1, 3, 5, 8, 10 or 12), of the 74LS07 
     - Connect the corresponding output pin (2, 4, 6, 9, 11 or 13) directly to the Clock signal of the IEC bus
     - Put a 1k resistor between the IEC bus Clock signal and +5V.
     - Do **not** remove the connection from the Clock signal to the microcontroller. This connection is still
       needed for reading the Clock signal.
  4. Do the same as in the step 3 for the Data signal.
  5. If you are using the SRQ line, also wire it through the driver just like Clock and Data. Note that there is no
     "SRQout" pin defined in the constructor because SRQ is a write-only signal from the perspective of the device.
     Instead connect the SRQ pin to the 74LS07 and make sure to remove the direct connection from the SRQ pin to the IEC bus.

After doing this your device should work the same as before but with the added protection of the line driver.


## Implementing a simple low-level device

Implementing a basic device using the IECDevice class requires three steps:
  1. Derive a new class from the IECDevice class and implement the device's behavior in the new class
  2. Create an instance of the IECBusHandler class and attach your device by calling attachDevice()
  3. Call the IECBusHandler::begin() and IECBusHandler::task() functions within your main sketch functions.

This section describes those steps based on the [IECBasicSerial](examples/IECBasicSerial/) 
example, a simple device that connects a serial (RS232) port to the IEC bus.

First we define a new class, derived from the IECDevice class. 

```
#include <IECDevice.h>
#include <IECBusHandler.h>

class IECBasicSerial : public IECDevice
{
 public:
  IECBasicSerial();

  virtual int8_t  canRead();
  virtual uint8_t read();

  virtual int8_t canWrite();
  virtual void   write(uint8_t data, bool eoi);
};
```

We implement the device functions by overriding the canRead/read/canWrite/write functions.
See the [IECDevice Class Reference](#iecdevice-class-reference) section below for a detailed description of these functions:

```
IECBasicSerial::IECBasicSerial() : IECDevice(6)
{}
```
The class constructor must call the IECDevice() constructor which defines the device's
address on the IEC bus (6).

```
int8_t IECBasicSerial::canRead() {
  uint8_t n = Serial.available();
  return n>1 ? 2 : n;
}
```
The canRead() function is called whenever data is requested from the device. For this device
we return 0 if we have nothing to send. This will cause a timeout error condition on the bus
(there is no provision in the IEC bus protocol for the computer to ask a device whether it
has data to send at all). On the computer side this will set bit 1 of the status word (i.e.
the ST variable in BASIC). If we returned -1 then canRead() would be called repeatedly, 
blocking the bus until we have something to send. That would prevent us from receiving incoming 
data on the bus.

```
uint8_t IECBasicSerial::read() { 
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
void IECBasicSerial::write(uint8_t data, bool eoi) { 
  Serial.write(data);
}
```
The write() function will **only** be called if the previous call to canWrite() returned 1. 
So at this point we know already that the serial port can accept data and just pass it on.

To implement our device class in a sketch we must instantiate the class and call the "begin()" and "task()"
functions:

```
IECBasicSerial iecSerial;
IECBusHandler iecBus(3, 4, 5);

void setup()
{
  Serial.begin(115200);
  iecBus.attachDevice(&iecSerial);
  iecBus.begin();
}

void loop()
{
  iecBus.task();
}
```

The IECBusHandler constructor defines the microcontroller pins (ATN=3, Clock=4, Data=5)
to which the IEC bus signals are connected.

We call the iecBus.attachDevice() function to attach our device to
the handler and then call the iecBus.begin() function to initialize the bus.
The iecBus.task() function must be called periodically as it 
handles the bus communication and calls our canRead/read/canWrite/write
functions when necessary.  See the [IECDevice Class Reference](#iecdevice-class-reference) 
section for a detailed description of these functions.

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

Implementing a file-based device using the IECFileDevice class requires three steps:
  1. Derive a new class from the IECFileDevice class and implement the device's behavior in the new class
  2. Create an instance of the IECBusHandler class and attach your device by calling attachDevice()
  3. Call the IECBusHandler::begin() and IECBusHandler::task() functions within your main sketch functions.

This section describes those steps based on the [IECBasicSD](examples/IECBasicSD/) 
example, a very simple device to read/write SD cards.
Note that this device will be limited in its functionality, it allows loading and saving programs
on the SD card but no other functionality (directory listing, status channel, deleting files etc..).
The purpose of this section is to demonstrate basic bus communication for file-based devices using the
IECFileDevice class. A more feature-complete implementation of a SD card reader is provided in
the [IECSD example](examples/IECSD). 

Note that any device derived from the IECFileDevice class automatically supports all supported fastload protocols.

First, a new class is defined and derived from the IECFileDevice class. 

```
#include <IECFileDevice.h>
#include <IECBusHandler.h>
#include <SdFat.h>

class IECBasicSD : public IECFileDevice
{
 public: 
  IECBasicSD();

 protected:
  virtual void begin();
  virtual bool open(uint8_t channel, const char *name);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t n, bool eoi);
  virtual void close(uint8_t channel);

 private:
  SdFat  m_sd;
  SdFile m_file;
};
```

We implement the device functions by overriding the open/read/write/close functions.
See the [IECFileDevice Class Reference](#iecfiledevice-class-reference) section below for a detailed description of these functions:

```
IECBasicSD::IECBasicSD() : IECFileDevice(9)
{}
```

The class constructor must call the IECDevice() constructor which defines the device's
address on the IEC bus (9).

```
void IECBasicSD::begin()
{
  m_sd.begin(10);
  IECFileDevice::begin();
}
```

The begin() function will be called by IECBusHandler. We use it to initialize the SD card 
interface, using pin 10 for CS. Note that we must also call the IECFileDevice::begin() 
function to properly initialize the device.

```
bool IECBasicSD::open(uint8_t channel, const char *name)
{
  return m_file.open(name, channel==0 ? O_RDONLY : (O_WRONLY | O_CREAT));
}
```

The "open()" function is called whenever the bus controller (computer) issues an OPEN command.
The function will return true if the open() call succeeded and false otherwise.

```
uint8_t IECBasicSD::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  return m_file.isOpen() ? m_file.read(buffer, bufferSize) : 0;
}
```

This function must fill the given buffer with up to bufferSize bytes of data from
the file that was previously opened for the given channel number. It must return the number of bytes 
written to the buffer. Returning 0 signals that no more data is left to read. 

```
uint8_t IECBasicSD::write(uint8_t channel, uint8_t *buffer, uint8_t n, bool eoi)
{
  return m_file.isOpen() ? m_file.write(buffer, n) : 0;
}
```

This function must write *n* data bytes from the buffer to the file that was previously opened 
for the given channel number and return the number of bytes written. Returning a number less 
than n signals an error condition.

```
void IECBasicSD::close(uint8_t channel)
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
IECBusHandler iecBus(3, 4, 5, 6);

void setup()
{
  iecBus.attachDevice(&iecSD);
  iecBus.begin();
}

void loop()
{
  iecBus.task();
}
```

The IECBusHandler constructor defines the microcontroller pins (ATN=3, Clock=4, Data=5, Reset=6) 
to which the IEC bus signals are connected.

We call the iecBus.attachDevice() function to attach our device to
the handler and then call the iecBus.begin() function to start the bus.
The iecBus.task() function must be called periodically as it 
handles the bus communication and calls our canRead/read/canWrite/write
functions when necessary.  See the [IECFileDevice Class Reference](#iecfiledevice-class-reference) 
section for a detailed description of these functions.

## IECBusHandler class reference

The IECBusHandler class facilitates the bus communication and will call the read/write functions
in its attached device(s) when necessary.

- ```IECBusHandler(uint8_t pinATN, uint8_t pinCLK, uint8_t pinDATA, uint8_t pinRESET = 0xFF, uint8_t pinCTRL = 0xFF, uint8_t pinSRQ = 0xFF)```  
  The IECBusHandler constructor defines the pins to which the IEC bus signals are connected. The pinRESET parameter is optional,
  if not given, the device will simply not respond to a bus reset. The pinCTRL parameter (also optional) is helpful
  for applications where the microcontroller may not be able to respond quickly enough to ATN requests 
  (see [Timing considerations](#timing-considerations) section below). Finally, the pinSRQ parameter (also optional)
  can be used to specify a pin for the SRQ functionality (see the [Wiring](#wiring) section above).

- ```bool attachDevice(IECDevice *dev)```  
  Attaches a new device to the bus handler. The maximum number of devices that can be attached is 
  defined in the ```IECConfig.h``` file (defaults to 4). If the IECBusHandler::begin() function has
  already been called (i.e. the bus is already active) then attachDevice() will also call the 
  device's begin() function.

- ```bool detachDevice(IECDevice *dev)```  
  Detaches a device from the bus handler. The bus handler will no longer respond to this device
  number on the bus and will no longer call the corresponding read/write functions in the device.
  Note that this does **not** delete the IECDevice object. 

- ```void begin()```  
  This function must be called once at startup, before the first call to "task". It will in turn
  call the begin() function of all devices that have been attached to the bus handler at this point.

- ```void task()```
  This function must be called periodically to handle the IEC bus communication.
  If the ATN signal is NOT connected to an interrupt-capable pin on your microcontroller
  then task() must be called at least once every millisecond. Otherwise you may get "Device not present"
  errors when trying to communicate with your device. If ATN is on an interrupt-capable
  pin less frequent calls are ok but bus communication will be slower if called less frequently.

- ```void setParallelPins(...)```
  This function can be called **before** attaching any devices to specify the pins to be used
  for the parallel cable. If not called, the default pins will be used. See the 
  [Parallel cable](#parallel-cable) section below for more information.


## IECDevice class reference

The IECDevice class has the following functions that may/must be called from your code:

- ```IECDevice(uint8_t devnum)```  
  Constructor for the IECDevice class, devnum is the IEC bus device number that the device should 
  respond to. Valid device numbers on the IEC bus range from 4 to 30.

- ```void setDeviceNumber(uint8_t devnum)```
  Changes the device's device number on the IEC bus. Valid device numbers on the IEC bus
  range from 4 to 30. This function can be called at any time, the device will respond to
  the new device number immediately at the next bus transmission.

- ```void setActive(bool active)```
  Allows to deactivate a device without detaching it from the bus entirely. An inactive
  device will not respond to any bus requests.

- ```void sendSRQ()```
  Allows the device to generate an SRQ interrupt in the bus controller (computer). 
  See the [Wiring](#wiring) section above.

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

- ```void enableSpeedDosSupport(bool enable)```  
  This function must be called **if** your device should support the SpeedDos parallel protocol.
  In most cases devices with SpeedDos support should be derived from the IECFileDevice class
  which handles SpeedDos support internally and you do not have to call enableSpeedDosSupport().
  For more information see the [SpeedDos support](#speeddos-support) section below.
  You can also use this function to disable SpeedDos support after it has been enabled.

The following functions can be overloaded in the derived device class to implement the device functions.
None of these function are *required*. For example, if your device only receives data then only the
canWrite() and write() functions need to be overloaded.

- ```void begin()```  
  This function will automatically be called by IECBusHandler::begin(), if the IECDevice is
  already attached to the IECBusHandler at that point. Otherwise it will be called when the
  device gets attached by the IECBusHandler::attachDevice() call.
  If you overload this function, make sure to call IECDevice::begin() from within your overloaded function.
- ```void task()```
  This function will automatically be called on every execution of IECBusHandler::task(), once for all attached devices. 
  It can be used to handle device-specific periodic tasks. 
  If you overload this function, make sure to call IECDevice::task() from within your overloaded function.
- ```int8_t canRead()```  
  This function will be called whenever the device is asked to send data
  to the bus controller (i.e. the computer). It should return one of four values: -1, 0, 1 or 2.  
  Returning -1 signals that we do not know yet whether there is more data to send.
  The canRead() function will be called again later and until then the bus will remain blocked.
  Alternatively, the canRead() function may wait on its own until an answer is known.  
  Returning 0 signals that there is no data to read.  
  Returning 1 signals that there is **exactly** one byte of data left to read.  
  Returning 2 signals that there are two or more bytes of data left to read.
- ```uint8_t read()```  
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
- ```void write(uint8_t data, bool eoi)``` 
  This function is called **only** if the previous call to canWrite() returned 1. The data argument
  is the data byte received on the bus. Note that the write() function must process the data and return 
  immediately (within 1 millisecond), otherwise bus timing errors may occur. 
  The eoi argument will be "true" if the host indicated that this is the last byte of a trasmission.
- ```void listen(uint8_t secondary)```  
  Called when the bus controller (computer) issues a LISTEN command, i.e. is about to send data to the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void unlisten()```  
  Called when the bus controller (computer) issues an UNLISTEN command, i.e. is done sending data.
- ```void talk(uint8_t secondary) ```  
  Called when the bus controller (computer) issues a TALK command, i.e. is requesting data from the device.
  This function must return immediately (within 1 millisecond), otherwise bus timing errors may occur. 
- ```void untalk()```  
  Called when the bus controller (computer) issues an UNTALK command, i.e. is done receiving data from the device.
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).
  If you overload this function, make sure to call IECDevice::reset() from within your overloaded function.


The following functions should be overloaded if the JiffyDos protocol should be supported.
In most cases devices with JiffyDos support should be derived from the IECFileDevice class
which handles JiffyDos support internally and you do not have to implement these functions.
For more information see the [JiffyDos support](jiffydos-support) section below.

- ```uint8_t peek()```  
  Called when the device is sending data using JiffyDos byte-by-byte protocol.  
  peek() will only be called if the last call to canRead() returned >0.  
  peek() should return the next character that will be read with read().  
  peek() is allowed to take an indefinite amount of time.  
- ```uint8_t read(uint8_t *buffer, uint8_t bufferSize)```  
  This function is called when the device is sending data using the JiffyDos block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, JiffyDos load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  

The following function should be overloaded if the Epyx FastLoad protocol should be supported.
In most cases devices with Epyx FastLoad support should be derived from the IECFileDevice class
which handles Epyx FastLoad support internally and you do not have to implement these functions.
For more information see the [Epyx FastLoad support](epyx-fastload-support) section below.

- ```uint8_t read(uint8_t *buffer, uint8_t bufferSize)```  
  This function is called when the device is sending data using the Epyx FastLoad block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, Epyx FastLoad load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  


The following functions should be overloaded if the DolphinDos parallel protocol should be supported.
In most cases devices with DolphinDos support should be derived from the IECFileDevice class
which handles DolphinDos support internally and you do not have to implement these functions.
For more information see the [DolphinDos support](#dolphindos-support) section below.

- ```uint8_t write(uint8_t *buffer, uint8_t bufferSize, bool eoi)```
  This function is only called when the device is receiving data using the DolphinDos block transfer (SAVE protocol).
  write() should process all the data in the buffer and return the number of bytes processed.
  Returning a number lower than bufferSize signals an error condition.
  The "eoi" parameter will be "true" if sender signaled that this is the final part of the transmission
  If write() is **not** overloaded, DolphinDos save performance will be several times slower than otherwise.
  write() is allowed to take an indefinite amount of time.
- ```uint8_t peek()```  
  Called when the device is sending data using DolphinDos byte-by-byte protocol.  
  peek() will only be called if the last call to canRead() returned >0.  
  peek() should return the next character that will be read with read().  
  peek() is allowed to take an indefinite amount of time.  
- ```uint8_t read(uint8_t *buffer, uint8_t bufferSize)```  
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

The following functions should be overloaded if the SpeedDos parallel protocol should be supported.
In most cases devices with SpeedDos support should be derived from the IECFileDevice class
which handles SpeedDos support internally and you do not have to implement these functions.
For more information see the [SpeedDos support](#speeddos-support) section below.

- ```uint8_t write(uint8_t *buffer, uint8_t bufferSize, bool eoi)```
  This function is only called when the device is receiving data using the SpeedDos block transfer (SAVE protocol).
  write() should process all the data in the buffer and return the number of bytes processed.
  Returning a number lower than bufferSize signals an error condition.
  The "eoi" parameter will be "true" if sender signaled that this is the final part of the transmission
  If write() is **not** overloaded, SpeedDos save performance will be several times slower than otherwise.
  write() is allowed to take an indefinite amount of time.
- ```uint8_t peek()```  
  Called when the device is sending data using SpeedDos byte-by-byte protocol.  
  peek() will only be called if the last call to canRead() returned >0.  
  peek() should return the next character that will be read with read().  
  peek() is allowed to take an indefinite amount of time.  
- ```uint8_t read(uint8_t *buffer, uint8_t bufferSize)```  
  This function is only called when the device is sending data using the SpeedDos block transfer (LOAD protocol).
  read() should fill the buffer with as much data as possible (up to bufferSize).
  read() must return the number of bytes put into the buffer
  If read() is **not** overloaded, SpeedDos load performance will be several times slower than otherwise.
  read() is allowed to take an indefinite amount of time.  

## IECFileDevice class reference

The IECFileDevice class has the following functions that may/must be called from your code:

- ```IECFileDevice(uint8_t devnum)```  
  Constructor for the IECFileDevice class, devnum is the IEC bus device number that the device should 
  respond to. Valid device numbers on the IEC bus range from 4 to 30.

- ```void setDeviceNumber(uint8_t devnum)```
  Changes the device's device number on the IEC bus. Valid device numbers on the IEC bus
  range from 4 to 30. This function can be called at any time, the device will respond to
  the new device number immediately at the next bus transmission.

- ```void setActive(bool active)```
  Allows to deactivate a device without detaching it from the bus entirely. An inactive
  device will not respond to any bus requests.

- ```void sendSRQ()```
  Allows the device to generate an SRQ interrupt in the bus controller (computer). 
  See the [Wiring](#wiring) section above.

The following functions can be overloaded in the derived device class to implement the device functions.
Only the open/close/read/write functions are required to be overloaded, others have default implementations
that will be used if not overloaded.

Most of these functions take an argument named *channel* which is the channel number given in
the OPEN command that opened the file on the computer side, i.e. ```OPEN fileNum, deviceNum, channel, name$```
The channel number will be in the range 0 to 14.  
Channel 15 is the status and command channel. If the computer reads from channel 15 on your device, 
the getStatus() function will be called and the result of that call will be sent to the computer.
If the computer writes to channel 15 on your device, the execute() function will be called that lets
the device process the command.

The channel number will be 0 when the computer executes a LOAD command and 1 when the computer 
executes a SAVE command.
  
- ```void begin()```  
  This function will automatically be called by IECBusHandler::begin(), if the IECDevice is
  already attached to the IECBusHandler at that point. Otherwise it will be called when the
  device gets attached by the IECBusHandler::attachDevice() call. It can be used to initialize
  device-specific parameters.
  If you overload this function, make sure to call IECFileDevice::begin() from within your overloaded function.
  Note that IECFileDevice::begin() will (among other things), enable all fastload protocols
  that are set to be enabled in ```IECConfig.h```. If you want your device to **not** support
  one or more protocols, call the ```enable*Support(false)``` functions in your device **after**
  calling IECFileDevice::begin(). 
- ```void task()```
  This function will automatically be called on every execution of IECBusHandler::task(), once for all attached devices. 
  It can be used to handle device-specific periodic tasks. 
  If you overload this function, make sure to call IECFileDevice::task() from within your overloaded function.
- ```bool open(uint8_t channel, const char *filename)```  
  This function is called whenever the bus controller (computer) issues an OPEN command.
  The *channel* parameter specifies the channel as described above and the *filename* 
  parameter is a zero-terminated string representing the file name given in the OPEN command.
  The function should return true if opening the file succeeded and false otherwise.
- ```void close(uint8_t channel)```  
  Close the file that was previously opened on *channel*. The close() function does not have a 
  return value to signal success or failure since the IEC bus protocol does not include a method 
  to transmit this information.  
- ```uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)```  
  Read up to *bufferSize* bytes of data from the file opened for *channel*, returning the number 
  of bytes read. There are two ways to signal end-of-data (EOI) to the receiver:
    - Returning a data length of 0.
    - Returning a data length of >0 and setting the *eoi parameter to true signals EOI after transmitting the data returned in this call.
  Note that read() may be called again even after signaling EOI once if the receiver requests
  more data 
- ```uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)```  
  Write *bufferSize* bytes of data to the file opened for *channel*, returning the number 
  of bytes written. Returning a number less than *bufferSize* signals an error condition.
- ```void getStatus(char *buffer, uint8_t bufferSize)```  
  Called when the computer reads from channel 15 and the status
  buffer is currently empty. This should populate *buffer* with an appropriate, zero-terminated
  status message of length up to *bufferSize*.
- ```void execute(const char *command, uint8_t cmdLen)```  
  Called when the computers sends data (i.e. a command) to channel 15.
  The *command* parameter is a 0-terminated string representing the command to execute,
  *commandLen* gives the full length of the received command which can be useful if
  the command itself may contain zeros.
- ```void reset()```  
  Called when a high->low edge is detected on the the IEC bus RESET signal line (only if pinRESET was given in the constructor).
  If you overload this function, make sure to call IECFileDevice::reset() from within your overloaded function.
  

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
   when calling the IECBusHandler constructor. The purpose of the circuit is to pull
   DATA low immediately when ATN goes low and only release it once the software
   confirms (via the CTRL pin) that it now is in control of the Data signal.

Apart from the ATN signal timing requirements there are a few functions in the 
IECDevice class that have limitations on how long they may take before returning.
Those are described in the [IECDevice Class Reference](#iecdevice-class-reference) section.

Devices derived from the IECFileDevice class have no other timing requirements apart from the ATN timing
as the IECFileDevice class handles all of them internally.

Finally, JiffyDos and Epyx FastLoad transfers require very precise timing which requires the IECDevice
library to disable all interrupts during such transfers. So be aware that if JiffyDos or Epyx FastLoad
support is enabled, the IECDevice::task() function may take up to to 20ms before returning and
with interrupts disabled during JiffyDos transfers. Devices derived from the IECFileDevice
class will automatically have JiffyDos and Epyx FastLoad support enabled. See the 
[JiffyDos support](jiffydos-support) and [Epyx FastLoad support](expyx-fastload-support) sections 
below for how to disable fastloader support if desired.

## JiffyDos support

The IECDevice class includes support for the [JiffyDos](https://www.go4retro.com/products/jiffydos/) 
bus protocol which significantly speeds up bus transfers, especially LOAD commands. 
The library automatically detects when the computer requests a JiffyDos transfer and responds correspondingly.

For high-level file-based devices (derived from the IECFileDevice class), all functionality
for JiffyDos support is already included in the IECFileDevice class. JiffyDos support is 
automatically enabled. In case you do NOT want your device to support JiffyDos, add
the following call in the begin() function of your derived class, **after** calling
IECFileDevice::begin(): ```enableJiffyDosSupport(false)```

To completely disable JiffyDos support (for example to save memory space on small controllers
like the Arduino UNO), comment out the "#define SUPPORT_JIFFY" line at the top of file ```src/IECConfig.h```.

For low-level devices (derived from the IECDevice class), two additional functions need to 
be overloaded: ```peek()``` must return the next data byte that will be retuned by a call
to ```read()``` and ```read(buffer, bufferSize)``` which when called should return 
a chunk of data to be transferred. See the [IECDevice class reference](#iecdevice-class-reference) section
for the full function definitions.
JiffyDos support is initially disabled for low-level devices and must be enabled by calling 
```enableJiffyDosSupport(true)``` in the begin() function of the derived class.

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
automatically enabled. In case you do NOT want your device to support FastLoad, add
the following call in the begin() function of your derived class, **after** calling
IECFileDevice::begin(): ```enableEpyxFastLoadSupport(false)```.

To completely disable FastLoad support (for example to save memory space on small controllers
like the Arduino UNO), comment out the "#define SUPPORT_EPYX" line at the top of file ```src/IECConfig.h```.

For low-level devices (derived from the IECDevice class), an additional functions need to 
be overloaded: ```read(buffer, bufferSize)``` should return a chunk of data to be transferred.
See the [IECDevice class reference](#iecdevice-class-reference) section for the full function definition.
Epyx FastLoad support is initially disabled for low-level devices and must be enabled by calling 
```enableEpyxFastLoadSupport(true)``` in the begin() function of the derived class.

As mentioned in the timing consideration section, interrupts will be disabled during FastLoad transfers 
which may cause your program to not be able to respond to interrupts for up to 20ms at a time.

Note that in order to use the FastLoad routins you need the Epyx FastLoad cartridge for your C64
(https://en.wikipedia.org/wiki/Epyx_Fast_Load). A modern replica of the cartridge can be purchased
at [TFW8B](https://www.tfw8b.com/product/epyx-fastload-reloaded-disk-sd2iec-turbo-loader-cartridge-c64).

## DolphinDos support

**Note:** Since DolphinDos support requires a number of additional pins for the parallel connection 
and DolphinDos is not a very widely used fast loader, it not enabled by default. To enable DolphinDos
support, un-comment the ```#define SUPPORT_DOLPHIN``` line at the top of file ```src/IECConfig.h```.

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
transmission speed. It will work without parallel cable but will not provide any speed improvement.
See the [Parallel cable](#iecdevice-class-reference) section for information on how to wire
a parallel cable to the C64 user port.

## SpeedDos support

**Note:** Since SpeedDos support requires a number of additional pins for the parallel connection 
and SpeedDos is not a very widely used fast loader, it not enabled by default. To enable SpeedDos
support, un-comment the ```#define SUPPORT_SPEEDDOS``` line at the top of file ```src/IECConfig.h```.

The IECDevice class includes support for the [SpeedDos](https://www.c64-wiki.com/wiki/SpeedDOS)
parallel protocol.

For high-level file-based devices (derived from the IECFileDevice class), all functionality
for SpeedDos support is already included in the IECFileDevice class. SpeedDos support is 
automatically enabled. In case you do NOT want your device to support SpeedDos, add
the following call in the begin() function of your derived class, **after** calling
IECFileDevice::begin(): ```enableSpeedDosSupport(false)```.

SpeedDos needs a replacement kernal in the C64 for its fast transmission routines.
The SpeedDos C64 kernal can be downloaded [here](https://csdb.dk/release/?id=21767&show=summary).

SpeedDos relies on a parallel connection between the computer and device for its improved
transmission speed. It will work without parallel cable but will not provide any speed improvement.
See the [Parallel cable](#iecdevice-class-reference) section for information on how to wire
a parallel cable to the C64 user port.

## Parallel cable

The SpeedDos and DolphinDos fastloaders require a parallel connection between the C64 user port
and the device. This section describes the necessary connections.

IECDevice has pre-defined pins for the different hardware platforms but those can be changed
by calling the ```setParallelPins(HT,HR,D0,D1,D2,D3,D4,D5,D6,D7)``` function. The pre-defined
pin numbers are as follows (note: the "Arduino Uno" column includes the R3 and R4 versions
as well as the Nano, Mini, Micro and Leonardo variants):

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

### Parallel cable using XRA1405 port extender

Some of the supported microcontrollers (Arduino Uno/Micro/Nano, ESP32) have a limited number
of available I/O pins which means that if the parallel cable is connected as shown above,
few (if any) pins are left available for other functions. To mitigate that, the IECDevice
library provides an option to use a port extender ([MaxLinear XRA1405](https://www.digikey.com/htmldatasheets/production/982544/0/0/1/xra1405.pdf))
for the 8-bit parallel data transmission.

The XRA1405 was chosen because:
  - It has 5V-tolerant inputs which means that 3.3V systems such as the Pi
    Pico and ESP32 do not require extra level conversion for the parallel data.
  - It connects to the microcontroller via SPI which allows very fast
    communication (24MHz). Also, since many use-cases for this library will
    already include an SD card reader, the SPI bus pins can be shared between
    the XRA1405 and the SD card reader, meaning that only three extra pins (CS, HR, HT)
    on the microcontroller are needed for the parallel cable connection.

Unfortunately, the XRA1405 is only available in QFN-24 a package, meaning some
non-trivial SMD soldering is required. I soldered an XRA1405 onto a [QFN-24
breakout board](https://www.digikey.com/en/products/detail/schmalztech-llc/ST-QFN-24-4X4-05/24617918) 
so it can be easily used on breadboards. The XRA1405 pin numbers given below
refer the the QFN-24 package.

The default connections for the XRA1405 to the microcontroller are as follows
(these can be changed by calling the setParallelPins() function).
The pin numbers in the "Arduino Uno" column apply to Arduino UNO R3 and R4
as well as Arduino Micro and Nano.

XRA1405 pin       | Signal | Arduino Uno | Raspberry Pi Pico | ESP32
------------------|--------|-------------|-------------------|------
18 (CS#)          | CS     | 9           | 20                | IO22
19 (SCL)          | SCK    | 13          | 18                | IO18
20 (SO)           | CIPO   | 12          | 16                | IO19
24 (SI)           | COPI   | 11          | 19                | IO23
21 (VCCP)         | 3.3V   | 3.3V        | 3.3V              | 3.3V
23 (VCC)          | 3.3V   | 3.3V        | 3.3V              | 3.3V
9 (GND)           | GND    | GND         | GND               | GND

C64 User Port pin | Signal | Arduino Uno | Raspberry Pi Pico | ESP32
------------------|--------|-------------|-------------------|------
8 (PC2)           | HR     | 2           | 15                | IO36 (VP)
B (FLAG2)         | HT     | 7           | 6                 | IO4

Finally, connect User Port pins C-L (PB0-PB7) to XRA1405 pins 1-8 (P0-P7).

To enable XRA1405 support in the library, un-comment the ```#define SUPPORT_PARALLEL_XRA1405```
line in file ```IECConfig.h```.

Note that the XRA1405 has two 8-bit I/O ports of which only the first one
is used by this library. You are free to use the other port (P8-P15) for your
own functions, for example by emplying the [XRA1405 library](https://github.com/NaveItay/XRA1405).

Finally, a few notes regarding 3.3V vs. 5V compatibility:
  - The XRA1405 chip CAN be connected directly to 5V microcontrollers (Arduino UNO) as
    it is 5V tolerant on its inputs. However, if you also connect an SD card reader then either
    make sure the SD card reader also operates at 3.3V (and is 5V tolerant) or provide 5V-to-3.3V
    level translation for (at least) the CIPO signal going from the XRA1405 to the microcontroller.
    Otherwise you will likely see read errors on the SD card, because the 3.3V CIPO output of the
    XRA1405 would be directly connected to the 5V CIPO output of the SD card reader, disrupting
    the signal. This is true even though those two outputs are never active at the same time
    (I suspect because of shunting diodes within the XRA1405).
  - For 3.3V microcontrollers, the XRA1405 will provide the level translation for the 8-bit
    data signals (PB0-PB7) but the PC2 and FLAG2 signals still are 5V signals and need
    level translation between the C64 and the microcontroller.


## Raspberry Pi Pico development board

Instructions for a development board to connect a Raspberry Pi Pico to a C64 are available 
[here](hardware/README.md#raspberry-pi-pico-development-board)

<table>
<tr><td><center>Connected using serial IEC cable only</center></td><td><center>Connected using serial IEC and parallel cable</center></td></tr>
<tr><td><img src="hardware/pictures/PiPicoSD.jpg"></td><td><img src="hardware/pictures/PiPicoSDParallel.jpg"></td></tr>
</table>
