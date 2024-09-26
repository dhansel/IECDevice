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


#ifndef IECFDC_H
#define IECFDC_H

#include <IECFileDevice.h>
#include "ff.h"

class IECFDC : public IECFileDevice
{
 public: 
  IECFDC(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL, byte pinLED);
  void begin(byte devnr = 0xFF);
  void task();

 protected:
  virtual void open(byte devnr, byte channel, const char *name);
  virtual byte write(byte devnr, byte channel, byte *buffer, byte bufferSize);
  virtual byte read(byte devnr, byte channel, byte *buffer, byte bufferSize);
  virtual void close(byte devnr, byte channel);
  virtual void getStatus(byte devnr, char *buffer, byte bufferSize);
  virtual void execute(byte devnr, const char *command, byte len);
  virtual void reset();

 private:
  void openFile(byte channel, const char *name);
  void openDir(const char *name);
  bool readDir(byte *data);

  void startDiskOp();
  void format(const char *name, bool lowLevel, byte interleave);
  char *findFile(char *fname);

  FATFS m_fatFs;
  FIL   m_fatFsFile;
  
  FRESULT m_ferror;
  byte m_errorTrack, m_errorSector;
  byte m_curCmd, m_pinLED;
  byte m_dir, m_dirBufferLen, m_dirBufferPtr;
};

#endif
