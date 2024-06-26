// -----------------------------------------------------------------------------
// 3.5"/5.25" DD/HD Disk controller for Arduino
// Copyright (C) 2021 David Hansel
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

#include "diskio.h"
#include "ArduinoFDC.h"

static uint32_t motorTimeout[2] = {0, 0};

static void disk_motor_on(BYTE pdrv)
{
  ArduinoFDC.selectDrive(pdrv);
  if( !ArduinoFDC.motorRunning() ) ArduinoFDC.motorOn();
  motorTimeout[pdrv] = millis()+3000;
}


void disk_motor_check_timeout()
{
  if( millis()>motorTimeout[0] ) { ArduinoFDC.selectDrive(0); ArduinoFDC.motorOff(); motorTimeout[0]=-1; }
  if( millis()>motorTimeout[1] ) { ArduinoFDC.selectDrive(1); ArduinoFDC.motorOff(); motorTimeout[1]=-1; }
}


DSTATUS disk_status(BYTE pdrv)
{ 
  // STA_NOINIT:  Drive not initialized
  // STA_NODISK:  No medium in the drive
  // STA_PROTECT: Write protected
  ArduinoFDC.selectDrive(pdrv);
  return ArduinoFDC.isWriteProtected() ? STA_PROTECT : 0;
}


DSTATUS disk_initialize(BYTE pdrv)
{ 
  ArduinoFDC.selectDrive(pdrv);
  return ((ArduinoFDC.haveDisk() ? 0 : (STA_NODISK|STA_NOINIT)) | 
          (ArduinoFDC.isWriteProtected() ? STA_PROTECT : 0));
}


DRESULT disk_read(BYTE pdrv, BYTE *buf, DWORD sec, UINT count)
{
  DRESULT res = RES_OK;

  disk_motor_on(pdrv);
  byte numhd  = ArduinoFDC.numHeads();
  byte numsec = ArduinoFDC.numSectors();

  if( count!=1 || sec>numhd*numsec*ArduinoFDC.numTracks() )
    res = RES_PARERR;
  else
    {
      byte head   = 0;
      byte track  = sec / (numsec*numhd);
      byte sector = sec % (numsec*numhd);
      if( sector >= numsec ) { head = 1; sector -= numsec; }

      byte r = S_NOHEADER, retry = 5;
      while( retry>0 && r!=S_OK ) { r = ArduinoFDC.readSector(track, head, sector+1, buf); retry--; }

      if( r==S_OK )
        {
          memmove(buf, buf+1, 512);
          res = RES_OK;
        }
      else if( r==S_NOTREADY )
        res = RES_NOTRDY;
      else
        res = RES_ERROR;
    }

  return res;
}


DRESULT disk_write(BYTE pdrv, BYTE *buf, DWORD sec, UINT count)
{
  DRESULT res = RES_OK;

  disk_motor_on(pdrv);
  byte numhd  = ArduinoFDC.numHeads();
  byte numsec = ArduinoFDC.numSectors();

  if( count!=1 || sec>numhd*numsec*ArduinoFDC.numTracks() )
    res = RES_PARERR;
  else
    {
      byte head   = 0;
      byte track  = sec / (numsec*numhd);
      byte sector = sec % (numsec*numhd);
      if( sector >= numsec ) { head = 1; sector -= numsec; }

      memmove(buf+1, buf, 512);
      byte r = S_NOHEADER, retry = 3;
      while( retry>0 && r!=S_OK ) { r = ArduinoFDC.writeSector(track, head, sector+1, buf, false); retry--; }
      memmove(buf, buf+1, 512);

      if( r==S_OK )
        res = RES_OK;
      else if( r==S_NOTREADY )
        res = RES_NOTRDY;
      else if( r==S_READONLY )
        res = RES_WRPRT;
      else
        res = RES_ERROR;
    }
  
  return res;
}


DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  DRESULT res = RES_ERROR;

  ArduinoFDC.selectDrive(pdrv);

  switch( cmd )
    {
    case CTRL_SYNC: 
      res = RES_OK; 
      break;

    case GET_SECTOR_COUNT:
      *((DWORD *) buff) = (DWORD) ArduinoFDC.numSectors() * (DWORD) ArduinoFDC.numTracks() * (DWORD) ArduinoFDC.numHeads();
      res = RES_OK;
      break;

    case GET_SECTOR_SIZE:
      *((DWORD *) buff) = 512;
      break;

    case GET_BLOCK_SIZE:
      *((DWORD *) buff) = 1;
      break;
    }
  
  return res;
}
