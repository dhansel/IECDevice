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


#ifndef CONVERTER_H
#define CONVERTER_H

#include <Arduino.h>
#include "IECCentronics.h"

#define PRINTER_READY    8
#define PRINTER_ERROR    4
#define PRINTER_PAPEROUT 2
#define PRINTER_SELECT   1

class Converter
{
 public:
  Converter() { m_controller = NULL; }

  // called when DIP setting is switched TO this mode
  virtual void begin() {}

  // called when DIP setting is switched AWAY from this mode
  virtual void end() {}

  // called to set current channel number
  virtual void setChannel(byte channel) {}

  // called for simple one-to-one byte conversion 
  virtual byte convert(byte inData) { return inData; }

  // called for more complex conversions => must dequeue and enqueue data manually
  // calls simple conversion function if not overridden
  virtual void convert() { if( canRead() && canWrite() ) write(convert(read())); }

  // called to update the status of the controller LED, default is for LED to be on
  // as long as the controller is able to receive data from the bus master
  virtual bool ledStatus() { return canReceive(); }

  // called when bus master reads the status channel (15), should fill buffer[]
  // a status message.
  virtual void getStatus(char buffer[], byte bufLen)
  {
    byte status = printerStatus();

    if( status & PRINTER_READY )
      strncpy_P(buffer, PSTR("00,READY\r"), bufLen);
    else if( status & PRINTER_PAPEROUT )
      strncpy_P(buffer, PSTR("03,NO PAPER\r"), bufLen);
    else if( status & PRINTER_ERROR ) 
      strncpy_P(buffer, PSTR("04,PRINTER ERROR\r"), bufLen);
    else if( !(status & PRINTER_SELECT) )
      strncpy_P(buffer, PSTR("01,OFF LINE\r"), bufLen);
    else
      strncpy_P(buffer, PSTR("05,UNKNOWN\r"), bufLen);
  }

  // called when bus master writes to the command channel (15)
  virtual void execCommand(const char *cmd) {}

  // called internally, do not override
  void init(IECCentronics *controller) { m_controller = controller; }

 protected:
  byte printerStatus() { return (m_controller->readShiftRegister() >> 4) ^ PRINTER_READY; }

  int  canRead()       { return m_controller->m_receive.availableToRead(); }
  byte read()          { return m_controller->m_receive.dequeue(); }
  bool canReceive()    { return !m_controller->m_receive.full(); }
  
  int  canWrite()      { return m_controller->m_send.availableToWrite(); }
  bool write(byte b)   { return m_controller->m_send.enqueue(b); }

 private:
  IECCentronics *m_controller;
};


#endif
