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

#ifndef IECCENTRONICS_H
#define IECCENTRONICS_H

#include <Arduino.h>
#include <IECDevice.h>

#define BUFSIZE 80

class Converter;

class SimpleQueue
{
 public:
  SimpleQueue()           { m_start = 0; m_end = 0; }

  bool empty()            { return m_end == m_start; }
  bool full()             { return byte(m_end+1) == m_start; }
  int  availableToRead()  { return (m_end-m_start) & 0xFF; }
  int  availableToWrite() { return m_start == m_end ? 256 : ((m_start-m_end-1)&0xFF); }
  bool enqueue(byte data) { return full()  ? false : (m_data[m_end++] = data, true); }
  byte dequeue()          { return empty() ? 0xFF  : m_data[m_start++]; }

 private:
  byte m_start, m_end, m_data[256];
};


class IECCentronics : public IECDevice
{
  friend void printerReadyISR();
  friend class Converter;

 public: 
  IECCentronics();
  void begin();
  void task();

  void setConverter(byte i, Converter *handler);

  bool printerBusy();
  bool printerOutOfPaper();
  bool printerError();
  bool printerSelect();

 protected:
  virtual void   listen(byte secondary);
  virtual void   talk(byte secondary);
  virtual void   unlisten();
  virtual int8_t canWrite();
  virtual void   write(byte data);
  virtual int8_t canRead();
  virtual byte   read();

 private:
  void printerReadySig();
  void sendByte(byte data);
  byte readShiftRegister();
  byte readDIP();
  bool printerReady();
  void handleInputMPS801();

  byte m_mode, m_channel;
  Converter *m_converters[8];

  byte m_cmdBufferLen, m_cmdBufferPtr, m_statusBufferLen, m_statusBufferPtr;
  char m_cmdBuffer[BUFSIZE], m_statusBuffer[BUFSIZE];

  bool m_ready;

  SimpleQueue m_receive, m_send;
};


#endif
