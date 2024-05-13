# IECCentronics

This is an adapter that allows attaching Centronics printers to the Commodore IEC bus.
Back in the day [solutions for this did exist](https://www.youtube.com/watch?v=g8cB98p5IS4) but
I wanted to print from my C64 to my Centronics printer, don't have an adapter and couldn't find 
a DIY solution so I came up with this. Actually this is where the IECDevice library started and
I generalized it as I came up with other ideas for devices.

This adapter is made to fill my specific need (attaching a Tandy DMP 130 printer to my C64 and
being able to use existing software with it) but it is designed such that it can be used with
any printer.

As is, the adapter has four main operating modes for relaying data received from the Commodore 
computer to the printer:
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

## Using IECCentronics

## Extending IECCentronics
