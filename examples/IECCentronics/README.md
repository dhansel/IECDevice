# IECCentronics

This is an adapter that allows attaching Centronics printers to the Commodore IEC bus.
Back in the day [solutions for this did exist](https://www.youtube.com/watch?v=g8cB98p5IS4) but
I wanted to print from my C64 to my Centronics printer, don't have an adapter and couldn't find 
a DIY solution so I came up with this. Actually this is where the IECDevice library started and
I generalized it as I came up with other ideas for devices.

As is, the adapter has four main operating modes for relaying data received from the Commodore 
computer to the connected printer. Modes 1-3 are generic and should work with any connected printer.
Mode 4 is specific to the Tandy (or Radio Shack) DMP 130 printer.

The IECCentronics software is designed such that it can easily be [extended](#extending-ieccentronics)
to support other data conversion methods or add support for different printers. The software
can simply be edited in the Arduino IDE and [uploaded](hardware/README.md#uploading-the-firmware) 
directly from there via the provided pin header on the board.

The currently implemented conversion modes are as follows:
1) **No conversion.** In this mode all received data is sent on to the printer without modifications.
This can be helpful if you want to run your own printer software that directly talks to the printer.
2) **Convert PETSCII->ASCII**. In this mode all data received from the computer is assumed to be text 
in Commodore's PETSCII encoding. While PETSCII is similar to ASCII there are significant differences 
regarding the character set. For example, lower-case and upper-case letters are reversed. This mode
corrects the case reversal such that the printed text looks as expected. Useful when printing
upper/lowercaset text.
3) **Convert PETSCII->uppercase ASCII**. This mode is very similar to the previous one except that
*all* characters received are converted to uppercase ASCII. This is useful for LISTing BASIC programs
as the LIST command sends all characters as lower-case but generally a listing looks more "normal"
when printed as upper-case.
4) **Emulate MPS801 on DMP130**. The previous conversion modes have one major drawback: since PETSCII
contains graphics characters that are just not part of the ASCII character set of Centronics printers,
text (or BASIC listings) containing those characters can not be properly printed. This mode fixed that
by switching the printer into graphics mode and printing all characters using the character set bitmap
from the Commodore MPS801 printer. The conversion mode also understands the text formatting and graphics
modes of the MPS801. This allows me to use my Tandy DMP130 as if it were an MPS801. For example, 
the output of the "Printer Test" program from the Commodore demo disk looks just as expected and other
software such as "Print Master" prints just fine.

Obviously mode #4 is specifically written for my Tandy (or Radio Shack) DMP130 printer and won't work
on others. But the software architecture of the IECCentronics device is designed to be easily extended
for other printers. For more details see section [Extending IECCentronics](#extending-ieccentronics) below.
  
## Building IECCentronics

Building an IECCentronics device is fairly straightforward. I am not selling any kits but all
necessary information (Gerber file, BOM, STL files for printing a case) is available in the
[hardware](hardware) directory.

![IECCentronics](pictures/pcb.jpg)

## Using IECCentronics

The device is plug-and-play - simply connect the IEC bus, USB power and printer to the device
and turn on your computer. Any data sent to the device's address (4 or 5, depending on the DIP
switch) will be sent to the printer. 

### DIP switches (conversion modes)

The DIP switches have the following functions:

- DIP switch 1 (leftmost): Device address (up=4, down=5)
- DIP switches 2, 3 and 4: Conversion mode selection (see table below)
  
DIP 2 | DIP 3 | DIP 4 | Mode
------|-------|-------|-----
up    | up    | up    | Direct pass-through (no conversion)
up    | up    | down  | Convert PETSCII to lowercase/uppercase ASCII
up    | down  | up    | Convert PETSCII to uppercase ASCII
up    | down  | down  | Emulate Commodore MPS801 on Tandy DMP130

If DIP 2 is set to "down" then converter modes 4-7 are selected which are currently
not implemented. Section [Extending IECCentronics](#extending-ieccentronics) below describes
how to implement new converters and assign them to conversion modes.

All DIP switches can be changed at runtime and the device will immediately start using the new 
settings (no reset or poweroff is required).

### Status channel

The IECCentronics device supports a status channel, similar to Commodore floppy disk drives. 
Reading from channel 15 will return one of the following status messages
- `00,READY`: Printer is ready to print
- `01,OFF LINE`: The printer is off line
- `03,NO PAPER`: The printer is out of paper
- `04,PRINTER ERROR`: The printer is reporting an error condition via its ERROR signal line

Due to the way the Centronics interface defines the signal levels for its various status pins
(SELECT, PE, FAULT), if no printer is connected the status will read as "03,NO PAPER". If a
printer is connected but turned off, the status depends on the internals of the printer but
will likely also report "NO PAPER".

### Jumper settings

The PCB has four different jumper settings that offer some more configuration options:
- `RESET`: If this jumper is installed then the IEC bus RESET will also reset IECCentronics.
  If not installed, only the on-board RESET button will cause a reset. Note that the on-board
  button does **not** cause a reset of other devices on the IEC bus.
- `RSTP`: If a jumper is connecting the middle and left pins then the printer will automatically
  receive a RESET signal whenever a RESET is seen on the IEC bus. If a jumper is connecting the
  middle and right pins then the printer's RESET line is controlled by pin 28 of the ATMega
  controller. With the current firmware this resets the printer when IECCentronics is reset but
  the function can easily be altered in the source code. If no jumper is installed then the
  printer is never reset.
- `SELECT`: If a jumper is installed then the *Select* pin (13) on the [printer port](https://www.lammertbies.nl/comm/cable/parallel) will be pulled LOW,
  otherwise it remains HIGH. Some printers require this to be LOW, others do not.
- `AUTOLF`: If a jumper is installed then the *Autofeed* pin (14) on the [printer port](https://www.lammertbies.nl/comm/cable/parallel) will be pulled LOW.
  otherwise it remains HIGH. On some printers this signal controls whether the printer automatically executes a line feed
  after receiving a carriage return (0Dh) character.

## Extending IECCentronics

The IECCentronics code is structured to make it easy to develop new conversion routines
for different printers. To add a new conversion mode, three steps are necessary:
1) Create a new converter class by deriving it from the Converter class
2) Implement a simple converter by overriding the "byte convert(byte data)" function
   or a more advanced converter by overriding the "void convert()" function in the new class.
3) Create an instance of the new converter class
4) Assign the new converter object to a conversion mode in the Arduino `begin()` function.

Up to eight different conversion modes can be defined and selected by the settings of DIP switches 2-4.
Mode # | DIP2 | DIP3 | DIP4
-------|------|------|-----
0      | up   | up   | up
1      | up   | up   | down
2      | up   | down | up
3      | up   | down | down
4      | down | up   | up
5      | down | up   | down
6      | down | down | up
7      | down | down | down

In the firmware included here only modes 0-3 are implemented as described above. You may either
change the existing implementations or add new ones for modes 4-7.

### Implementing a simple (single-byte) converter

An implementation of a simple converter can be seen in the [IECCentronics.ino](IECCentronics.ino) file
by the implementation of the PETSCII-to-ASCII converter:

First a new class is created, derived from the Converter class:
```
class ConverterPETSCIIToASCII : public Converter
{
 public:
  byte convert(byte data);
};
```

The only function defined in that class is "convert()" which takes an input data byte and
returns the converted data byte. In this case, swapping the codes for uppercase and lowercase letters:

```
byte ConverterPETSCIIToASCII::convert(byte data)
{
  if( data>=192 ) 
    data -= 96;

  if( data>=65 && data<=90 )
    data += 32;
  else if( data>=97 && data<=122 )
    data -= 32;
  
  return data;
}
```

Next we create an instance of the converter (note that this must be outside of `setup()`)
```
ConverterPETSCIIToASCII mode1;
```

And finally (within the `setup()` function) we set the new converter as conversion mode #1
(see the table above for the relation between conversion mode numbers and DIP switch settings):
```
iec.setConverter(1, &mode1);
```

That's all that needs to be done. If DIP switches 2-4 are set to "up,up,down" any byte
received by IECCentronics will pass through the "convert" function of class ConverterPETSCIItoASCII 
before being sent to the printer.

### Implementing an advanced (multi-byte) converter

For more complex conversions that involve more than one character (e.g. character sequences),
the conversion function above is not very helpful since it only converts one character at a time.

In thise cases you can override the `void convert()` function. This function is called periodically,
independent of when data has been received from the computer. Within your convert() function you
can call the `canRead()`, `read()`, `canWrite()` and `write()` functions to get any data received
and schedule data to send.

As a simple example, the default implementation of the `void convert()` function is this:
```
void convert() 
{
  if( canRead() && canWrite() ) write(convert(read()));
}
```
If data has been received from the computer **and** we have space to schedule output data to the printer then
read the input data, call the `byte convert(byte data)` function and schedule its result to be sent to the printer.

A more complex example can be found in the implementation of the `void convert()` function in the
[ConverterTandyMPS801](https://github.com/dhansel/IECDevice/blob/5724cff6040543991a420c1624d976b8e8a48e9b/examples/IECCentronics/ConverterTandyMPS801.cpp#L264) class.

The general framework to implementing a converter using this method is the same as from the previous
(simple converter) section. The only change is which one of the two `convert` functions gets overridden.

Note that if you override the `void convert()` function then `byte convert(byte data)` will not
be called automatically. All conversion must happen in `void convert()` in that case.

### Converter class reference

The `Converter` class has the following functions that can be overridden in the derived class.
Note that overriding all these functions is optional, although you would want to override one
of the `convert` functions, otherwise your converter will just pass the input through to the
printer as is.

- `byte convert(byte data)` This function is called whenever a character is has been received 
  from the computer. The **data** value is the received data and the return value is what will
  be sent to the printer.
- `void convert()` This function is called periodically and can be used to implement more advanced
  conversions involving more than one byte of data. It should call the `canRead, read, canWrite, write`
  functions (see below) to perform the actual conversions.
- `void begin()` This function is called when the converter becomes active, either at startup or
  when the DIP switch setting is changed *to* this converter.
- `void end()` This function is falled when the DIP switch seting is changed *away from* this converter.
- `void setChannel(byte channel)` This function is called when the computer starts sending data to the device.
  The **channel** parameter contains the secondary address used in the corresponding *OPEN* command
  on the computer (e.g. if `OPEN 1,4,7` was used then the channel number is 7).
- `bool ledStatus()` This function is called repeatedly to update the status of the LED on the IECCentronics
  board. Returning true will turn the LED on. If this function is not overridden then the LED will be
  on as long as the receive buffer if not full.
- `void getStatus(char buffer[], byte buflen)` This function is called when the user queries the status channel (channel 15). 
  It should fill the **buffer** with a status message of up to **buflen** characters. This message is then
  sent back to the computer. If this function is not overridden then the status messages will match the
  ones described in the *Status Channel* section above.
- `void execCommand(char *command)` This function is called if the computer sends a command on the
  command channel (channel 15). Overriding it enables you to process such commands, for example allowing
  the computer to programmatically change the behavior of the converter.

Aside from the functions that can be overridden, the `Converter` class also provides a number of helper
functions for the actual conversion. These are mostly helpful within the `void convert()` function:

- `int canRead()` Returns the number of data bytes currently in the *receive* buffer, i.e. bytes that
  have been sent by the computer and not yet processed for conversion. The receive buffer is 256 bytes long.
- `byte read()` Returns the next data byte in the receive buffer. Returns 255 if the receice buffer is empty.
- `int canWrite()` Returns the number of available bytes in the send buffer. The send buffer contains data
  that has been scheduled to be sent to the printer but has not yet been sent. The send buffer is 256 bytes long.
- `bool write(byte data)` Schedules a byte of data to be sent to the printer. Returns *false* if the send buffer
  is full and can not accept more data, otherwise returns *true*.
