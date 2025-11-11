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
  IECFDC(uint8_t devnr, uint8_t pinLED);
  virtual void begin();
  virtual void task();

 protected:
  virtual bool open(uint8_t channel, const char *name, uint8_t nameLen);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual void close(uint8_t channel);
  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual void execute(const char *command, uint8_t len);
  virtual void reset();

 private:
  void openFile(uint8_t channel, const char *name);
  void openDir(const char *name);
  bool readDir(uint8_t *data);

  void startDiskOp();
  void format(const char *name, bool lowLevel, uint8_t interleave);
  char *findFile(char *fname);

  FATFS m_fatFs;
  FIL   m_fatFsFile;
  
  FRESULT m_ferror;
  uint8_t m_errorTrack, m_errorSector;
  uint8_t m_curCmd, m_pinLED;
  uint8_t m_dir, m_dirBufferLen, m_dirBufferPtr;
};

#endif
