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
#include "Converter.h"
#include "ConverterTandyMPS801.h"

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
  iec.begin();

  iec.setConverter(1, &mode1);
  iec.setConverter(2, &mode2);
  iec.setConverter(3, &mode3);
  iec.setConverter(4, &mode4);
}


void loop()
{
  iec.task();
}
