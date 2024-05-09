# IECFDC

This example combines the IECDevice library with my [ArduinoFDC library](https://github.com/dhansel/ArduinoFDC)
to connect PC-style disk dives (3.5" and 5.25") to a Commodore computer,
enabling the computer to read and write MFM disks.

This example is specifically adapted to the limited resources in an Arduino UNO and therefore
has some limitations as to which operations are supported (see below). For a version
of this with more features see the [IECFDCMega](https://github.com/dhansel/IECDevice/tree/main/examples/IECFDCMega) example.

## Wiring

To wire the disk drive I recommend using the [ArduinoFDC shield](https://github.com/dhansel/ArduinoFDC#arduinofdc-shields).
Due to the limited number of I/O pins on the Arduino Uno, this example uses the SelectB and MotorB
pins for other functions (only one disk drive is supported). Make sure to cut the traces for MotorB
and SelectB on the shield as described in the ArduinoFDC shield documentation.

The following table lists the IEC bus pin connections for this example (NC=not connected).

IEC Bus Pin | Signal   | Arduino Uno
------------|----------|------------
1           | SRQ      | NC         
2           | GND      | GND        
3           | ATN      | 3          
4           | CLK      | 4          
5           | DATA     | 5          
6           | RESET    | RST

The ArduinoFDC library disables interrupts while reading or writing the disk which can take several
milliseconds and can causes IEC bus timing issues as described in the 
[Timing Considerations](https://github.com/dhansel/IECDevice#timing-considerations) section of the IECDevice library.
Therefore this example code utilizes the hardware extension described in that section. Wire the 74LS125 IC 
up as described in that section onto the prototyping area on the ArduinoFDC shield. The CTRL pin must be
connected to the Arduino's "A1" pin.

To show disk drive activity and status (i.e. blinking for an errlr), wire an LED from A2 of the Arduino 
through a 150 Ohm resistor to GND.

Finally, you can wire the IEC bus RESET signal to the RST pin of the Arduino. Doing so will reset the
Arduino whenever the computer is reset.

Fully assembled IECFDC device:
<img src="IECFDC.jpg" width="50%">   
