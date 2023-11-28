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


#include <Arduino.h>
#include <IECDevice.h>

#define DEVICE_NUMBER 6

#define PIN_ATN   3
#define PIN_CLK   4
#define PIN_DATA  5


// -----------------------------------------------------------------------------
// IECBasicSerial class implements a very basic IEC-to-serial converter
// -----------------------------------------------------------------------------


class IECBasicSerial : public IECDevice
{
 public:
  IECBasicSerial() : IECDevice(PIN_ATN, PIN_CLK, PIN_DATA) {}

  virtual int8_t canRead(byte channel);
  virtual byte   read(byte channel);

  virtual int8_t canWrite(byte channel);
  virtual void   write(byte channel, byte data);
};


int8_t IECBasicSerial::canWrite(byte channel)
{
  // Return -1 if we can't write right now which will cause this to be
  // called again until we eiher return 0 or 1.
  // Alternatively we could just wait within this function until we are ready.
  return Serial.availableForWrite()>0 ? 1 : -1;
}


void IECBasicSerial::write(byte secondary, byte data)
{ 
  // write() will only be called if canWrite() returned >0.
  Serial.write(data);
}


int8_t IECBasicSerial::canRead(byte channel)
{
  // Return 0 if we have nothing to read. If we returned -1 then the bus
  // would be blocked until we have something to read, which would prevent
  // us from receiving data.
  byte n = Serial.available();
  return n>1 ? 2 : n;
}


byte IECBasicSerial::read(byte secondary)
{ 
  // read() will only be called if canRead() returned >0.
  return Serial.read();
}


// -----------------------------------------------------------------------------


IECBasicSerial iecSerial;

void setup()
{
  // initialize serial communication
  Serial.begin(115200);

  // initialize IEC bus
  iecSerial.begin(DEVICE_NUMBER);
}


void loop()
{
  // handle IEC bus communication (this will call the read and write functions above)
  iecSerial.task();
}
