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
  IECFDC(uint8_t devnr, uint8_t pinLED);
  virtual void begin();
  virtual void task();

 protected:
  virtual void open(uint8_t channel, const char *name);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize);
  virtual void close(uint8_t channel);
  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual void execute(const char *command, uint8_t len);
  virtual void reset();

 private:
  void openFile(FIL *f, uint8_t channel, const char *name);
  void openDir(FIL *f, const char *name);
  bool readDir(FIL *f, uint8_t *data);
  void openRawDir(FIL *f, const char *name);
  bool readRawDir(FIL *f, uint8_t *data);

  void startDiskOp(uint8_t drive);
  void format(uint8_t drive, const char *name, bool lowLevel, uint8_t interleave);
  char *findFile(FIL *f, char *fname);

  const char *getDriveSpec(uint8_t unit);
  const char *getCurrentDriveSpec();
  bool parseCommand(const char *prefix, const char **command, uint8_t *drive);
  const char *prefixDriveSpec(uint8_t drive, const char *name, uint8_t maxNameLen = 0xFF);
  uint8_t getAvailableFileIdx();
  FIL *getAvailableFile();

  FATFS   m_fatFs[2];
  FIL     m_fatFsFile[IECFDC_MAXFILES];
  FILINFO m_fatFsFileInfo;
  
  FRESULT m_ferror;
  uint8_t m_errorTrack, m_errorSector, m_numOpenFiles;
  uint8_t m_pinLED, m_curDrive, m_channelFiles[16];
  uint16_t m_dirBufferLen, m_dirBufferPtr;
};

#endif
