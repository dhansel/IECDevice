// -----------------------------------------------------------------------------
// Copyright (C) 2023 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------


#include "IECCentronics.h"
#include "IECBusHandler.h"
#include "Converter.h"
#include "ConverterTandyMPS801.h"

#define PIN_ATN         3 // PD3
#define PIN_CLK_IN      8 // PB0
#define PIN_DATA_IN    12 // PB4
#define PIN_CLK_OUT     7 // PD7
#define PIN_DATA_OUT    6 // PD6
#define PIN_RESET       4 // PD4


IECBusHandler iecBus(PIN_ATN, PIN_CLK_IN, PIN_CLK_OUT, PIN_DATA_IN, PIN_DATA_OUT, PIN_RESET);
IECCentronics iec;


// -------------------------------------------------------------------


class ConverterPETSCIIToASCII : public Converter
{
 public:
  byte convert(byte data);
};


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


// -------------------------------------------------------------------


class ConverterPETSCIIToASCIIupper : public Converter
{
 public:
  byte convert(byte data);
};


byte ConverterPETSCIIToASCIIupper::convert(byte data)
{
  // convert PETSCII input to uppercase ASCII output
  if( data>=192 ) 
    data -= 96;

  if( data>=97 && data<=122 )
    data -= 32;

  return data;
}


// -------------------------------------------------------------------

ConverterPETSCIIToASCII      mode1;
ConverterPETSCIIToASCIIupper mode2;
ConverterTandyMPS801         mode3;
Converter mode4;

void setup()
{
  iecBus.attachDevice(&iec);
  iecBus.begin();

  iec.setConverter(1, &mode1);
  iec.setConverter(2, &mode2);
  iec.setConverter(3, &mode3);
  iec.setConverter(4, &mode4);
}


void loop()
{
  iecBus.task();
}
