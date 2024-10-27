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

#if defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)
#define BUFFER_SIZE 128
#endif

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
  virtual void open(byte devnr, byte channel, const char *name) {}

  // close file on channel
  virtual void close(byte devnr, byte channel) {}

  // write bufferSize bytes to file on channel, returning the number of bytes written
  // Returning less than bufferSize signals "cannot receive more data" for this file
  virtual byte write(byte devnr, byte channel, byte *buffer, byte bufferSize) { return 0; }

  // read up to bufferSize bytes from file in channel, returning the number of bytes read
  // returning 0 will signal end-of-file to the receiver. Returning 0
  // for the FIRST call after open() signals an error condition
  // (e.g. C64 load command will show "file not found")
  virtual byte read(byte devnr, byte channel, byte *buffer, byte bufferSize) { return 0; }

  // called when the bus master reads from channel 15 and the status
  // buffer is currently empty. this should populate buffer with an appropriate 
  // status message bufferSize is the maximum allowed length of the message
  virtual void getStatus(byte devnr, char *buffer, byte bufferSize) { *buffer=0; }

  // called when the bus master sends data (i.e. a command) to channel 15
  // command is a 0-terminated string representing the command to execute
  // commandLen contains the full length of the received command (useful if
  // the command itself may contain zeros)
  virtual void execute(byte devnr, const char *command, byte cmdLen) {}

  // called on falling edge of RESET line
  virtual void reset();

  // can be called by derived class to set the status buffer (dataLen max 32 bytes)
  void setStatus(byte devnr, char *data, byte dataLen);

  // can be called by derived class to clear the status buffer, causing readStatus()
  // to be called again the next time the status channel is queried
  void clearStatus(byte devnr) { setStatus(devnr, NULL, 0); }

#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
  virtual bool epyxReadSector(byte track, byte sector, byte *buffer);
  virtual bool epyxWriteSector(byte track, byte sector, byte *buffer);
#endif

 private:

  virtual void talk(byte devnr, byte secondary);
  virtual void listen(byte devnr, byte secondary);
  virtual void untalk();
  virtual void unlisten();
  virtual int8_t canWrite(byte devnr);
  virtual int8_t canRead(byte devnr);
  virtual void write(byte devnr, byte data, bool eoi);
  virtual byte write(byte devnr, byte *buffer, byte bufferSize, bool eoi);
  virtual byte read(byte devnr);
  virtual byte read(byte devnr, byte *buffer, byte bufferSize);
  virtual byte peek(byte devnr);

  void fileTask();
  bool checkMWcmd(uint16_t addr, byte len, byte checksum) const;

  bool   m_opening, m_canServeATN;
  byte   m_channel, m_cmd;
  char   m_nameBuffer[41];

  // On smaller platforms (e.g. Arduino Uno) the indexing done for multiple devices significantly
  // eats into the flash memory usage. Since on those platforms most times we only want to have
  // one device anyways we use scalars for MAX_DEVICES==1 even though it would work just fine with arrays.
#if MAX_DEVICES==1
  byte   m_dataBuffer[15][2];
  int8_t m_statusBufferLen, m_statusBufferPtr, m_nameBufferLen, m_dataBufferLen[15];
  char   m_statusBuffer[32];
#else
  byte   m_dataBuffer[MAX_DEVICES][15][2];
  int8_t m_statusBufferLen[MAX_DEVICES], m_statusBufferPtr[MAX_DEVICES], m_nameBufferLen, m_dataBufferLen[MAX_DEVICES][15];
  char   m_statusBuffer[MAX_DEVICES][32];
#endif

#if BUFFER_SIZE>0
#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
  byte   m_buffer[256];
#else
  byte   m_buffer[BUFFER_SIZE];
#endif
#endif

#ifdef SUPPORT_EPYX
  byte m_epyxCtr;
#endif
};


#endif
