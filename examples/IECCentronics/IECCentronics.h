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
  SimpleQueue()              { m_start = 0; m_end = 0; }

  bool empty()               { return m_end == m_start; }
  bool full()                { return uint8_t(m_end+1) == m_start; }
  int  availableToRead()     { return (m_end-m_start) & 0xFF; }
  int  availableToWrite()    { return m_start == m_end ? 256 : ((m_start-m_end-1)&0xFF); }
  bool enqueue(uint8_t data) { return full()  ? false : (m_data[m_end++] = data, true); }
  uint8_t dequeue()          { return empty() ? 0xFF  : m_data[m_start++]; }

 private:
  uint8_t m_start, m_end, m_data[256];
};


class IECCentronics : public IECDevice
{
  friend void printerReadyISR();
  friend class Converter;

 public: 
  IECCentronics();

  void setConverter(uint8_t i, Converter *handler);

  bool printerBusy();
  bool printerOutOfPaper();
  bool printerError();
  bool printerSelect();

 protected:
  virtual void    begin();
  virtual void    task();
  virtual void    listen(uint8_t secondary);
  virtual void    talk(uint8_t secondary);
  virtual void    unlisten();
  virtual int8_t  canWrite();
  virtual void    write(uint8_t data, bool eoi);
  virtual int8_t  canRead();
  virtual uint8_t read();

 private:
  void printerReadySig();
  void sendByte(uint8_t data);
  uint8_t readShiftRegister();
  uint8_t readDIP();
  bool printerReady();
  void handleInputMPS801();

  uint8_t m_mode, m_channel;
  Converter *m_converters[8];

  uint8_t m_cmdBufferLen, m_cmdBufferPtr, m_statusBufferLen, m_statusBufferPtr;
  char m_cmdBuffer[BUFSIZE], m_statusBuffer[BUFSIZE];

  bool m_ready;

  SimpleQueue m_receive, m_send;
};


#endif
