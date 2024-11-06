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


#ifndef IECFDCMEGA_H
#define IECFDCMEGA_H

#include <IECFileDevice.h>
#include "ff.h"

#define IECFDC_MAXFILES 4

class IECFDC : public IECFileDevice
{
 public: 
  IECFDC(byte devnr, byte pinLED);
  virtual void begin();
  virtual void task();

 protected:
  virtual void open(byte channel, const char *name);
  virtual byte write(byte channel, byte *buffer, byte bufferSize);
  virtual byte read(byte channel, byte *buffer, byte bufferSize);
  virtual void close(byte channel);
  virtual void getStatus(char *buffer, byte bufferSize);
  virtual void execute(const char *command, byte len);
  virtual void reset();

 private:
  void openFile(FIL *f, byte channel, const char *name);
  void openDir(FIL *f, const char *name);
  bool readDir(FIL *f, byte *data);
  void openRawDir(FIL *f, const char *name);
  bool readRawDir(FIL *f, byte *data);

  void startDiskOp(byte drive);
  void format(byte drive, const char *name, bool lowLevel, byte interleave);
  char *findFile(FIL *f, char *fname);

  const char *getDriveSpec(byte unit);
  const char *getCurrentDriveSpec();
  bool parseCommand(const char *prefix, const char **command, byte *drive);
  const char *prefixDriveSpec(byte drive, const char *name, byte maxNameLen = 0xFF);
  byte getAvailableFileIdx();
  FIL *getAvailableFile();

  FATFS   m_fatFs[2];
  FIL     m_fatFsFile[IECFDC_MAXFILES];
  FILINFO m_fatFsFileInfo;
  
  FRESULT m_ferror;
  byte m_errorTrack, m_errorSector, m_numOpenFiles;
  byte m_pinLED, m_curDrive, m_channelFiles[16];
  word m_dirBufferLen, m_dirBufferPtr;
};

#endif
