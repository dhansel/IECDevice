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

#ifndef IECFILEDEVICE_H
#define IECFILEDEVICE_H

#include "IECDevice.h"


class IECFileDevice : public IECDevice
{
 public:
  IECFileDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF);

  // must be called once at startup before the first call to "task", devnr
  // is the IEC bus device number that this device should react to
  virtual void begin(byte devnr);

  // must be called periodically to handle IEC bus communication
  virtual void task();

 protected:

  // --- override the following functions in your device class:

  // open file "name" on channel
  virtual void open(byte channel, const char *name) {}

  // close file on channel
  virtual void close(byte channel) {}

  // write one byte to file on channel, return "true" if successful
  // Returning "false" signals "cannot receive more data" for this file
  virtual bool write(byte channel, byte data) { return false; }

  // read one byte from file in channel, return "true" if successful
  // "false" will signal end-of-file to the receiver. Returning "false"
  // for the FIRST byte after open() signals an error condition
  // (e.g. C64 load command will show "file not found")
  virtual bool read(byte channel, byte *data) { return false; }

  // called when the bus master reads from channel 15 and the status
  // buffer is currently empty. this should populate buffer with an appropriate 
  // status message bufferSize is the maximum allowed length of the message
  virtual void getStatus(char *buffer, byte bufferSize) { *buffer=0; }

  // called when the bus master sends data (i.e. a command) to channel 15
  // command is a 0-terminated string representing the command to execute
  // commandLen contains the full length of the received command (useful if
  // the command itself may contain zeros)
  virtual void execute(const char *command, byte cmdLen) {}

  // called on falling edge of RESET line
  virtual void reset();

  // can be called by derived class to set the status buffer (dataLen max 32 bytes)
  void setStatus(char *data, byte dataLen);

 private:

  virtual void talk(byte secondary);
  virtual void listen(byte secondary);
  virtual void untalk();
  virtual void unlisten();
  virtual int8_t canWrite();
  virtual int8_t canRead();
  virtual void write(byte data);
  virtual byte read();

  void fileTask();

  bool m_opening, m_canServeATN;
  byte m_channel, m_cmd, m_dataBuffer[15][2];
  char m_statusBuffer[32], m_statusBufferLen, m_statusBufferPtr, m_nameBufferLen, m_dataBufferLen[15];
  char m_nameBuffer[33];
};


#endif
