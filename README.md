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

# IECDevice class

(Take a look at the [IECBasicSerial](examples/IECBasicSerial/IECBasicSerial.ino) example to see a simple but functioning device implementation).

To implement a basic low-level device, create a new C++ class derived from IECDevice.

```
class MyIECDevice : public IECDevice
{
 public:
  MyIECDevice(pinATN, pinCLK, pinDATA, pinRESET) : IECDevice(pinATN, pinCLK, pinDATA, pinRESET) {}

  virtual int8_t canRead();
  virtual byte   read();

  virtual int8_t canWrite();
  virtual void   write(byte data);
};
```

There are five functions defined in the IECDevice class that can be overridden in your 
device class: the IECDevice() constructor, canRead(), read(), canWrite() and write().

- The constructor of your class must call the IECDevice() constructor. ```IECDevice(pinATN, pinCLK, pinDATA, pinRESET)``` 
defines the pins to which the IEC bus signals care connected. The pinRESET parameter is optional. If not given, the device
will simply not respond to a bus reset.

- ```canRead()``` This function will be called whenever the device is asked to send data
to the bus controller (i.e. the computer). It should return one of four values: -1, 0, 1 or 2  
Returning -1 signals that we do not know yet whether there is more data to send.
The canRead() function will be called again later and until then the bus will remain blocked.
Alternatively, the canRead() function may wait on its own until an answer is known.  
Returning 0 signals that there is no data to read.  
Returning 1 signals that there is **exactly** one byte of data left to read.  
Returning 2 signals that there are two or more bytes of data left to read.
- ```read()``` This function is called **only** if the previous call to canRead() returned a value greater than 0.
read() must return the next data byte.
- ```canWrite()``` This function will be called whenever the bus controller (computer) sends data
to your device. It should return one of three values: -1, 0 or 1
Returning -1 signals that we do not know yet whether we can accept more data.
The canWrite() function will be called again later and until then the bus will remain blocked.
Alternatively, the canWrite() function may wait on its own until an answer is known.  
Returning 0 signals that we are not able to accept more data.  
Returning 1 signals that we can accept data.  
canWrite() should **only** return 1 if the device is ready to receive and process the data immediately.
- ```write(data)``` is called **only** if the previous call to canWrite() returned 1. The data argument
is the data byte received on the bus. Note that the write() function must process the data and return 
immediately (within 1 millisecond), otherwise bus timing errors may occur. 

To implement your device class in a sketch you must instantiate the class and call the "begin()" and "task()"
functions. The basic structure of a sketch implementing an IECDevice should look something like this:

```
MyIECDevice myDevice(3,4,5);

void setup()
{
   myDevice.begin(6);
   // ... other code
}

void loop()
{
   // ... other code
   myDevice.task();
}
```

Apart from its constructor, the IECDevice class has only two public functions that need to be called:
- ```IECDevice::begin(devnr)``` must be called once at startup (i.e. in your code's "setup" function).
It defines the device number to which the device will react on the bus. You may call the function again
later to change the device number.
- ```IECDevice::task()``` must be called repeatedly (i.e. in your code's "loop" function) since it
implements the actual bus protocol. In most cases it is not critical to call task() more than once
every millisecond but us communication may run slower if tasK() is not called often enough.
See section "Timing considerations" below.

# Timing considerations
