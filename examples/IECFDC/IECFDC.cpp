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

#include "IECFDC.h"
#include "IECDevice.h"
#include "ArduinoFDC.h"
#include <EEPROM.h>
#include "ff.h"

#define FR_SPLASH    ((FRESULT) 0xFF)
#define FR_SCRATCHED ((FRESULT) 0xFE)
#define FR_EXTSTAT   ((FRESULT) 0xFD)

// defined in diskio.c
void disk_motor_check_timeout();

#define DEBUG 0


IECFDC::IECFDC(uint8_t devnr, uint8_t pinLED) : IECFileDevice(devnr)
{
  m_pinLED = pinLED;
}


void IECFDC::begin()
{
#if DEBUG>0
  Serial.begin(115200);
#endif

  // get device number from EEPROM
  m_devnr = EEPROM.read(0);
  if( m_devnr<3 || m_devnr>15 ) { m_devnr = 9; EEPROM.write(0, m_devnr); }

  IECFileDevice::begin();

  if( m_pinLED<0xFF ) 
    {
      pinMode(m_pinLED, OUTPUT);
      digitalWrite(m_pinLED, HIGH);
      delay(500);
      digitalWrite(m_pinLED, LOW);
    }

  m_ferror = FR_SPLASH;
  m_errorTrack = 0;
  m_errorSector = 0;
  m_dir = 0;

  ArduinoFDC.begin((enum ArduinoFDCClass::DriveType) min(EEPROM.read(1), 4));
  memset(&m_fatFs, 0, sizeof(FATFS));
  memset(&m_fatFsFile, 0, sizeof(FIL));

  f_mount(&m_fatFs, "0:", 0);
}


void IECFDC::task()
{
  // handle status LED
  if( m_pinLED<0xFF )
    {
      static unsigned long nextblink = 0;
      if( m_ferror!=FR_OK && m_ferror!=FR_SPLASH && m_ferror!=FR_EXTSTAT && m_ferror!=FR_SCRATCHED )
        {
          if( millis()>nextblink )
            {
              digitalWrite(m_pinLED, !digitalRead(m_pinLED));
              nextblink += 500;
            }
        }
      else
        {
          digitalWrite(m_pinLED, m_fatFsFile.obj.fs!=0 || m_dir);
          nextblink = 0;
        }
    }

  // check whether it is time to turn off the disk motor (in diskio.cpp)
  disk_motor_check_timeout();

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();
}


void IECFDC::startDiskOp()
{
  // if the disk has changed then re-mount it
  if( ArduinoFDC.diskChanged() )
    f_mount(&m_fatFs, "0:", 0);

  // if we have an LED then turn it on now
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);
}


void IECFDC::format(const char *name, bool lowLevel, uint8_t interleave)
{
  MKFS_PARM param;
  param.fmt = FM_FAT | FM_SFD; // FAT12 type, no disk partitioning
  param.n_fat = 2;             // number of FATs
  param.n_heads = 2;           // number of heads
  param.n_sec_track = ArduinoFDC.numSectors(); 
  param.align = 1;             // block alignment (not used for FAT12)
          
  switch( ArduinoFDC.getDriveType() )
    {
    case ArduinoFDCClass::DT_5_DD:
    case ArduinoFDCClass::DT_5_DDonHD:
      param.au_size = 1024; // bytes/cluster
      param.n_root  = 112;  // number of root directory entries
      param.media   = 0xFD; // media descriptor
      break;
              
    case ArduinoFDCClass::DT_5_HD:
      param.au_size = 512;  // bytes/cluster
      param.n_root  = 224;  // number of root directory entries
      param.media   = 0xF9; // media descriptor
      break;

    case ArduinoFDCClass::DT_3_DD:
      param.au_size = 1024; // bytes/cluster
      param.n_root  = 112;  // number of root directory entries
      param.media   = 0xF9; // media descriptor
      break;

    case ArduinoFDCClass::DT_3_HD:
      param.au_size = 512;  // bytes/cluster
      param.n_root  = 224;  // number of root directory entries
      param.media   = 0xF0; // media descriptor
      break;
    }

  f_unmount("0:");
  if( !lowLevel || (ArduinoFDC.formatDisk(m_fatFs.win, 0, 255, interleave))==S_OK )
    {
      m_ferror = f_mkfs ("0:", &param, m_fatFs.win, 512);
      if( m_ferror==FR_OK ) 
        {
          m_ferror = f_mount(&m_fatFs, "0:", 0);
#if FF_USE_LABEL>0
          if( m_ferror==FR_OK ) m_ferror = f_setlabel(name);
#endif
        }
    }
  else
    m_ferror = FR_DISK_ERR;
}


char *IECFDC::findFile(char *fname)
{
#if DEBUG>0
  Serial.print(F("FINDFILE: ")); Serial.print(fname);
#endif

  if( fname[0]==':' ) fname++;

  DIR *dir = (DIR *) m_fatFsFile.buf;
  FILINFO *fileInfo = (FILINFO *) (m_fatFsFile.buf+sizeof(DIR));

  m_ferror = f_findfirst(dir, fileInfo, "0:", fname);
  if( m_ferror==FR_OK )
    {
      if( fileInfo->fname[0]==0 )
        {
#if DEBUG>0
          Serial.print(F(" => NOT FOUND: ")); Serial.print(m_ferror);
#endif
          m_ferror = FR_NO_FILE;
        }
      else
        {
#if DEBUG>0
          Serial.print(F(" => FOUND: ")); Serial.print(fileInfo->fname);
#endif
          strcpy(fname, fileInfo->fname);
        }

      f_closedir(dir);
    }
  
  return fname;
}


void IECFDC::openDir(const char *name)
{
#if DEBUG>0
  Serial.println(F("DIR"));
#endif
  startDiskOp();

  DIR     *dir       = (DIR  *)  m_fatFsFile.buf;
  char    *dirBuffer = (char *) (m_fatFsFile.buf+sizeof(DIR)+sizeof(FILINFO));

  m_ferror = f_opendir(dir, "0:");
  if( m_ferror == FR_OK )
    {
      m_dir = 1;
      dirBuffer[0] = 0x01;
      dirBuffer[1] = 0x08;
      m_dirBufferLen = 2;
      m_dirBufferPtr = 0;

      dirBuffer[m_dirBufferLen++] = 1;
      dirBuffer[m_dirBufferLen++] = 1;
      dirBuffer[m_dirBufferLen++] = 0;
      dirBuffer[m_dirBufferLen++] = 0;
      dirBuffer[m_dirBufferLen++] = 18;
      dirBuffer[m_dirBufferLen++] = '"';
      int n = m_dirBufferLen;
#if FF_USE_LABEL>0
      f_getlabel("0:", dirBuffer+m_dirBufferLen, NULL); // label is 11 characters max
#else
      dirBuffer[m_dirBufferLen]=0;
#endif
      m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
      f_getcwd(dirBuffer+m_dirBufferLen, 64-8-m_dirBufferLen);
      dirBuffer[m_dirBufferLen]=0;
      dirBuffer[64-8] = 0;
      m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
      if( dirBuffer[m_dirBufferLen-1]=='/' ) dirBuffer[--m_dirBufferLen]=0; 
      n = 16-strlen(dirBuffer+n); // pad name to 16 characters
      while( n-->0 ) dirBuffer[m_dirBufferLen++] = ' ';
      strcpy_P(dirBuffer+m_dirBufferLen, PSTR("\" 00 2A"));
      m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
      dirBuffer[m_dirBufferLen++] = 0;
    }
}


bool IECFDC::readDir(uint8_t *data)
{
  DIR     *dir       = (DIR *)      m_fatFsFile.buf;
  FILINFO *fileInfo  = (FILINFO *) (m_fatFsFile.buf+sizeof(DIR));
  char    *dirBuffer = (char    *) (m_fatFsFile.buf+sizeof(DIR)+sizeof(FILINFO));

  if( m_dir && m_dirBufferPtr>=m_dirBufferLen )
    {
      m_dirBufferLen = 0;
      m_dirBufferPtr = 0;

      m_ferror = f_readdir(dir, fileInfo);
      if( m_ferror!=FR_OK )
        {
          f_closedir(dir);
          m_dir = 0;
        }
      else if( fileInfo->fname[0]==0 )
        {
          DWORD free;
          FATFS *fs = dir->obj.fs;

          f_getfree("0:", &free, &fs);
          free = free*fs->csize*2;
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = free&255;
          dirBuffer[m_dirBufferLen++] = free/256;
          strcpy_P(dirBuffer+m_dirBufferLen, PSTR("BLOCKS FREE."));
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen)+1;
          dirBuffer[m_dirBufferLen++] = 0;
          dirBuffer[m_dirBufferLen++] = 0;
          f_closedir(dir);
          m_dir = 0;
        }
      else
        {
          uint16_t size = fileInfo->fsize==0 ? 0 : min(fileInfo->fsize/254+1, 65535);
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = size&255;
          dirBuffer[m_dirBufferLen++] = size/256;
          if( size<10 )    dirBuffer[m_dirBufferLen++] = ' ';
          if( size<100 )   dirBuffer[m_dirBufferLen++] = ' ';
          if( size<1000 )  dirBuffer[m_dirBufferLen++] = ' ';
          //if( size<10000 ) dirBuffer[m_dirBufferLen++] = ' ';

          dirBuffer[m_dirBufferLen++] = '"';
          strcpy(dirBuffer+m_dirBufferLen, fileInfo->fname); // file name is 12 characters max ("12345678.EXT")
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
          dirBuffer[m_dirBufferLen++] = '"';
          dirBuffer[m_dirBufferLen] = 0;
                
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
          int n = 17-strlen(fileInfo->fname);
          while(n-->0) dirBuffer[m_dirBufferLen++] = ' ';
          strcpy_P(dirBuffer+m_dirBufferLen, (fileInfo->fattrib & AM_DIR) ? PSTR("DIR") : PSTR("PRG"));
          m_dirBufferLen+=4;
        }
    }
  
  if( m_dirBufferPtr<m_dirBufferLen )
    {
      *data = dirBuffer[m_dirBufferPtr++];
      return true;
    }
  else
    return false;
}


void IECFDC::openFile(uint8_t channel, const char *name)
{
  uint8_t fileIdx = 0;
  uint8_t mode = FA_READ;
  m_ferror = FR_OK;

  if( channel==0 )
    mode = FA_READ;
  else if( channel==1 )
    mode = FA_WRITE;
  else
    {
      char *comma = strchr((char *) name, ',');
      if( comma!=NULL )
        {
          *comma = 0;
          //ftype  = *(comma+1);
          comma  = strchr(comma+1, ',');
          if( comma!=NULL )
            {
              if( *(comma+1)=='R' )
                mode = FA_READ;
              else if( *(comma+1)=='W' )
                mode = FA_WRITE;
              else
                m_ferror = FR_INVALID_PARAMETER;
            }
        }
    }

  if( m_ferror==FR_OK )
    {
      startDiskOp();

      if( mode==FA_WRITE )
        {
          if( name[0]=='@' && name[1]==':' )
            { name+=2; mode |= FA_CREATE_ALWAYS; }
          else if( name[0]=='@' && name[1]!=0 && name[2]==':' )
            { name++; mode |= FA_CREATE_ALWAYS; }
          else
            mode |= FA_CREATE_NEW;
        }

      if( name[1]==':' ) name+=2;

      m_ferror = f_open(&m_fatFsFile, name, mode);
      if( m_ferror != FR_OK && mode==FA_READ )
        {
          char *fname = findFile((char *) name);
          if( m_ferror == FR_OK )
            m_ferror = f_open(&m_fatFsFile, fname, FA_READ);
        }
    }

#if DEBUG>0
  if( m_ferror == FR_OK )
    Serial.println(F(" => OK"));
  else
    { Serial.print(F(" => FAIL:")); Serial.println(m_ferror); }
#endif
}


bool IECFDC::open(uint8_t channel, const char *name)
{
  // The "~" (0x7E) used by FAT in shortened file names translates
  // to the "pi" symbol in PETSCII (when listing the directory).
  // But when "pi" is sent as part of a file name it arrives as 0xFF
  for(uint8_t *c = (uint8_t *) name; *c!=0; c++)
    if( *c == 0xFF ) *c = 0x7E;

  if( m_fatFsFile.obj.fs!=0 || m_dir )
    m_ferror = FR_TOO_MANY_OPEN_FILES;
  else if( name[0]!='$' )
    openFile(channel, name);
  else
    openDir(name);

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  return m_ferror == FR_OK;
}


uint8_t IECFDC::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  if( m_fatFsFile.obj.fs!=0 )
    {
      UINT count;
      m_ferror = f_read(&m_fatFsFile, buffer, bufferSize, &count);
      if( m_ferror!=FR_OK || count==0 ) { f_close(&m_fatFsFile); count=0; }
      return count;
    }
  else
    return readDir(buffer) ? 1 : 0;
}


uint8_t IECFDC::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  uint8_t res = 0;

  if( m_fatFsFile.obj.fs!=0 )
    {
      UINT count;
      m_ferror = f_write(&m_fatFsFile, buffer, bufferSize, &count);
      if( m_ferror != FR_OK ) 
        f_close(&m_fatFsFile);
      else
        res = count;
    }

  return res;
}


void IECFDC::close(uint8_t channel)
{
  if( m_fatFsFile.obj.fs!=0 )
    f_close(&m_fatFsFile);
  else if( m_dir )
    {
      f_closedir((DIR *) m_fatFsFile.buf);
      m_dir = 0;
    }
}


void IECFDC::execute(const char *command, uint8_t len)
{
  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  // The "~" (0x7E) used by FAT in shortened file names translates
  // to the "pi" symbol in PETSCII (when listing the directory).
  // But when "pi" is sent as part of a file name it arrives as 0xFF
  for(uint8_t *c = (uint8_t *) command; *c!=0; c++)
    if( *c == 0xFF ) *c = 0x7E;

  if( command[0]=='U' )
    {
      if( command[1]==':' || command[1]=='J' )
        reset();
    }
  else if( command[0]=='X' || command[0]=='E' )
    {
      m_ferror = FR_EXTSTAT;
      command++;

      if( command[0]=='T' )
        {
          const char *buf = command+1;
          if( *buf==':' || *buf=='=' ) buf++;

          if( buf[0]>='0' && buf[0]<='4' )
            {
              uint8_t tp = (buf[0]-'0');
              ArduinoFDC.setDriveType(0, (enum ArduinoFDCClass::DriveType) tp);
              EEPROM.write(1, tp);
            }
          else
            m_ferror = FR_INVALID_PARAMETER;
        }
      else if( command[0]>='1' && command[0]<='9' )
        {
          const char *c = command;
          uint8_t devnr = *c++ - '0';
          if( *c>='0' && *c<='9' ) devnr = devnr*10 + *c++ - '0';
          if( *c!=0 && (*c=='!' && *(c+1)!=0) ) devnr = 0;

          if( devnr>2 && devnr<16 )
            {
              if( *c=='!' ) EEPROM.write(0, devnr);
              m_devnr = devnr;
            }
          else
            m_ferror = FR_INVALID_PARAMETER;              
        }
      else if( command[0]!=0 )
        m_ferror = FR_INVALID_PARAMETER;              
    }
  else if( strncmp_P(command, PSTR("S:"), 2)==0 )
    {
      uint8_t n = 0;
      startDiskOp();

      if( m_fatFsFile.obj.fs==0 )
        {
          DIR *dir = (DIR *) m_fatFsFile.buf;
          FILINFO *fileInfo = (FILINFO *) (m_fatFsFile.buf+sizeof(DIR));

          m_ferror = f_findfirst(dir, fileInfo, "0:", command+2);
          while( m_ferror==FR_OK && fileInfo->fname[0]!=0 )
            {
              m_ferror = f_unlink(fileInfo->fname);
              if( m_ferror==FR_OK )
                { n++; m_ferror = f_findnext(dir, fileInfo); }
            }
        }
      else
        m_ferror = FR_TOO_MANY_OPEN_FILES;
            
      if( m_ferror==FR_OK ) { m_ferror = FR_SCRATCHED; m_errorTrack = n; }
    }
  else if( strcmp_P(command, PSTR("I"))==0 )
    {
      startDiskOp();
      m_ferror = f_mount(&m_fatFs, "0:", 1);
    }
  else if( strncmp_P(command, PSTR("N:"), 2)==0 )
    {
#if DEBUG==0
      char *comma = strchr(command, ',');
      if( comma!=NULL ) *comma = 0;
      startDiskOp();
      // interleave of 7 determined experimentally for maximum load
      // performance with JiffyDOS on a 3.5" HD disk
      format(command+2, comma!=NULL, 7);
#else
      // not enough program space for DEBUG and format routine
      m_ferror = FR_NOT_ENOUGH_CORE;
#endif
    }
  else if( strncmp_P(command, PSTR("MD:"), 3)==0 )
    {
      startDiskOp();
      m_ferror = f_mkdir(command+3);
      }
  else if( strncmp_P(command, PSTR("RD:"), 3)==0 )
    {
      startDiskOp();
      m_ferror = f_unlink(command+3);
    }
  else if( strncmp_P(command, PSTR("CD:"), 3)==0 )
    m_ferror = f_chdir(command+3);
  else if( strcmp_P(command, PSTR("CD_"))==0 )
    m_ferror = f_chdir("..");
  else
    m_ferror = FR_INVALID_PARAMETER;
}


void IECFDC::getStatus(char *buffer, uint8_t bufferSize)
{
  uint8_t code = 0;
  const char *message = NULL;
  switch( m_ferror )
    {
    case FR_OK:                  { code = 0;  message = PSTR(" OK"); break; }
    case FR_SCRATCHED:           { code = 1;  message = PSTR("FILES SCRATCHED"); break; }
    case FR_EXTSTAT:             { code = 2;  message = PSTR("T=0"); break; }
    case FR_NOT_READY:
    case FR_NO_FILESYSTEM:
    case FR_DISK_ERR:            { code = 74; message = PSTR("DRIVE NOT READY"); m_errorTrack = m_ferror; break; }
    case FR_NO_PATH:
    case FR_NO_FILE:             { code = 62; message = PSTR("FILE NOT FOUND"); m_errorTrack = m_ferror; break; }
    case FR_EXIST:               { code = 63; message = PSTR("FILE EXISTS"); break; }
    case FR_WRITE_PROTECTED:     { code = 26; message = PSTR("WRITE PROTECT ON"); break; }
    case FR_INVALID_NAME:
    case FR_INVALID_DRIVE:
    case FR_INVALID_PARAMETER:   { code = 33; message = PSTR("SYNTAX ERROR"); m_errorTrack = m_ferror; break; }
    case FR_NOT_ENOUGH_CORE:     { code = 95; message = PSTR("OUT OF MEMORY"); break; }
    case FR_TOO_MANY_OPEN_FILES: { code = 96; message = PSTR("TOO MANY OPEN FILES"); break; }
    case FR_SPLASH:              { code = 73; message = PSTR("IECFDC V0.1"); break; }
    default:                     { code = 99; message = PSTR("INTERNAL ERROR"); m_errorTrack = m_ferror; break; }
    }

  if( code<2 || code==73 ) m_errorSector = 0;
  uint8_t i = 0;
  buffer[i++] = '0' + (code / 10);
  buffer[i++] = '0' + (code % 10);
  buffer[i++] = ',';
  strncpy_P(buffer+i, message, bufferSize-11);
  i += strlen_P(message);
  if( code==2 ) buffer[i-1] = '0'+ArduinoFDC.getDriveType(0);
  buffer[i++] = ',';
  buffer[i++] = '0' + (m_errorTrack / 10);
  buffer[i++] = '0' + (m_errorTrack % 10);
  buffer[i++] = ',';
  buffer[i++] = '0' + (m_errorSector / 10);
  buffer[i++] = '0' + (m_errorSector % 10);
  buffer[i++] = '\r';
  buffer[i] = 0;

  m_ferror = FR_OK;
  m_errorTrack = 0;
  m_errorSector = 0;
}


void IECFDC::reset()
{
  IECFileDevice::reset();

  if( m_dir ) { f_closedir((DIR *) m_fatFsFile.buf); m_dir = 0; }
  if( m_fatFsFile.obj.fs!=0 ) f_close(&m_fatFsFile);

  m_devnr = EEPROM.read(0);
  m_ferror = FR_SPLASH;
  m_errorTrack = 0;
  m_errorSector = 0;

  if( m_pinLED<0xFF ) 
    {
      digitalWrite(m_pinLED, HIGH);
      delay(250);
      digitalWrite(m_pinLED, LOW);
    }
}
