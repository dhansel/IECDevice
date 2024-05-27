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


#include "IECFDCMega.h"
#include "ArduinoFDC.h"
#include <EEPROM.h>
#include "ff.h"

#define FR_SPLASH     ((FRESULT) 0xFF)
#define FR_SCRATCHED  ((FRESULT) 0xFE)
#define FR_EXTSTAT    ((FRESULT) 0xFD)
#define FR_NO_CHANNEL ((FRESULT) 0xFC)
#define FR_MEMOP      ((FRESULT) 0xFB)

static const char *driveTypes[] = { "5DD", "5DDHD", "5HD", "3DD", "3HD", "8SS", "8DS", NULL };

// defined in diskio.c
void disk_motor_check_timeout();

#define DEBUG 1


const char *IECFDC::getDriveSpec(byte unit)
{
  static char drive[3] = {'0', ':', 0};
  drive[0] = (unit==0) ? '0' : '1';
  return drive;
}


const char *IECFDC::getCurrentDriveSpec()
{
  return getDriveSpec(m_curDrive);
}



IECFDC::IECFDC(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL, byte pinLED) :
  IECFileDevice(pinATN, pinCLK, pinDATA, pinRESET, pinCTRL)
{
  m_pinLED = pinLED;
}


void IECFDC::begin(byte devnr)
{
#if DEBUG>0
  Serial.begin(115200);
#endif

  if( devnr==0xFF )
    {
      devnr = EEPROM.read(0);
      if( devnr<3 || devnr>15 ) { devnr = 9; EEPROM.write(0, devnr); }
    }
  IECFileDevice::begin(devnr);
  if( m_pinLED<0xFF ) pinMode(m_pinLED, OUTPUT);

  m_curDrive = EEPROM.read(1);
  if( m_curDrive>1 ) m_curDrive = 0;

  byte t0 = min(EEPROM.read(2), 6);
  byte t1 = min(EEPROM.read(3), 6);
  ArduinoFDC.begin((enum ArduinoFDCClass::DriveType) t0, (enum ArduinoFDCClass::DriveType) t1);

  memset(&m_fatFs, 0, sizeof(FATFS)*2);
  memset(&m_fatFsFile, 0, sizeof(FIL)*IECFDC_MAXFILES);
  f_mount(&(m_fatFs[0]), getDriveSpec(0), 0);
  f_mount(&(m_fatFs[1]), getDriveSpec(1), 0);
  f_chdrive(getDriveSpec(m_curDrive));

  m_ferror = FR_SPLASH;
  reset();
}


void IECFDC::task()
{
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
          digitalWrite(m_pinLED, m_numOpenFiles>0);
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
  ArduinoFDC.selectDrive(0);
  if( ArduinoFDC.diskChanged() ) f_mount(&(m_fatFs[0]), "0:", 0);
  ArduinoFDC.selectDrive(1);
  if( ArduinoFDC.diskChanged() ) f_mount(&(m_fatFs[1]), "1:", 0);
 
  // if we have an LED then turn it on now
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);
}


void IECFDC::format(byte drive, const char *name, bool lowLevel, byte interleave)
{
  MKFS_PARM param;

  ArduinoFDC.selectDrive(drive);

  param.fmt = FM_FAT | FM_SFD; // FAT12 type, no disk partitioning
  param.n_fat = 2;             // number of FATs
  param.n_heads = ArduinoFDC.numHeads(); 
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
    case ArduinoFDCClass::DT_8_DD_SS:
    case ArduinoFDCClass::DT_8_DD_DS:
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

  f_unmount(getDriveSpec(drive));
  if( !lowLevel || (ArduinoFDC.formatDisk(m_fatFs[drive].win, 0, 255, interleave))==S_OK )
    {
      m_ferror = f_mkfs(getDriveSpec(drive), &param, m_fatFs[drive].win, 512);
      if( m_ferror==FR_OK ) 
        {
          m_ferror = f_mount(&(m_fatFs[drive]), getDriveSpec(drive), 0);
          if( m_ferror==FR_OK ) 
            {
              char buf[14];
              strcpy(buf, getDriveSpec(drive));
              strncpy(buf+2, name, 11);
              buf[13]=0;
              m_ferror = f_setlabel(buf);
            }
        }
    }
  else
    m_ferror = FR_DISK_ERR;
}


char *IECFDC::findFile(FIL *f, char *fname)
{
#if DEBUG>0
  Serial.print(F("FINDFILE: ")); Serial.print(fname);
#endif

  DIR *dir = (DIR *) f->buf;
  FILINFO *fileInfo = (FILINFO *) (f->buf+sizeof(DIR));

  if( fname[0]==':' ) fname++;

  m_ferror = f_findfirst(dir, fileInfo, getCurrentDriveSpec(), fname);
  if( m_ferror==FR_OK )
    {
      if( fileInfo->fname[0]==0 )
        {
#if DEBUG>0
          Serial.print(F("=> NOT FOUND: ")); Serial.println(m_ferror);
#endif
          m_ferror = FR_NO_FILE;
        }
      else
        {
#if DEBUG>0
          Serial.print(F("=> FOUND: ")); Serial.println(fileInfo->fname);
#endif
          strcpy(fname, fileInfo->fname);
        }

      f_closedir(dir);
    }
  
  return fname;
}


bool IECFDC::parseCommand(const char *prefix, const char **command, byte *drive)
{
  byte i = strlen(prefix);
  if( strncmp(*command, prefix, i)==0 )
    {
      if( (*command)[i]==':' )
        { *drive = m_curDrive; i++; }
      else if( (*command)[i]==0 )
        *drive = m_curDrive;
      else if( ((*command)[i]=='0' || (*command)[i]=='1') && ((*command)[i+1]==':' || (*command)[i+1]==0) )
        { *drive = (*command)[i]-'0'; i += 2; }
      else
        return false;

      *command += i;
      return true;
    }
  else
    return false;
}


const char *IECFDC::prefixDriveSpec(byte drive, const char *name, byte maxNameLen)
{
  static char buf[35];
  if( maxNameLen>32 ) maxNameLen = 32;
  
  strcpy(buf, getDriveSpec(drive));
  strncpy(buf+2, name, maxNameLen);
  buf[2+maxNameLen] = 0;

  return buf;
}


byte IECFDC::getAvailableFileIdx()
{
  for(byte i=0; i<IECFDC_MAXFILES; i++)
    if( m_fatFsFile[i].obj.fs==0 )
      return i;

  return IECFDC_MAXFILES;
}


FIL *IECFDC::getAvailableFile()
{
  byte i = getAvailableFileIdx();
  return i<IECFDC_MAXFILES ? &(m_fatFsFile[i]) : NULL;
}


void IECFDC::openDir(FIL *f, const char *name)
{
  DIR     *dir        = (DIR  *)  f->buf;
  char    *nameBuffer = (char *) (f->buf+sizeof(DIR));
  char    *dirBuffer  = (char *) (f->buf+sizeof(DIR)+34);

#if DEBUG>0
  Serial.println(F("OPEN-DIR"));
#endif

  byte drive = m_curDrive;
  name++;
  if( *name=='0' || *name=='1' ) drive = *name++ - '0';
  if( *name==':' ) 
    name++;
  else
    name = "*";
  
  startDiskOp();
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
  strcpy(dirBuffer+m_dirBufferLen, getDriveSpec(drive));
  m_ferror = f_getlabel(getDriveSpec(drive), dirBuffer+m_dirBufferLen+2, NULL); // label is 11 characters max
  if( m_ferror==FR_OK )
    {
      m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
      if( drive!=m_curDrive ) f_chdrive(getDriveSpec(drive));
      m_ferror = f_getcwd(dirBuffer+m_dirBufferLen, 64-8-m_dirBufferLen);
      if( m_ferror==FR_OK )
        {
          if( drive!=m_curDrive ) f_chdrive(getDriveSpec(m_curDrive));
          dirBuffer[64-8] = 0;
          memmove(dirBuffer+m_dirBufferLen, dirBuffer+m_dirBufferLen+2, strlen(dirBuffer+m_dirBufferLen+2)+1); // erase leading "0:" of current directory
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
          if( dirBuffer[m_dirBufferLen-1]=='/' ) dirBuffer[--m_dirBufferLen]=0; 
          n = 16-strlen(dirBuffer+n); // pad name to 16 characters
          while( n-->0 ) dirBuffer[m_dirBufferLen++] = ' ';
          strcpy_P(dirBuffer+m_dirBufferLen, PSTR("\" 00 2A"));
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen);
          dirBuffer[m_dirBufferLen++] = 0;
        }

      // save file name pattern
      nameBuffer[0] = '0'+drive;
      strncpy(nameBuffer+1, name, 32);
      nameBuffer[33] = 0;
    }
}



bool IECFDC::readDir(FIL *f, byte *data)
{
  DIR     *dir        = (DIR *)   f->buf;
  char    *nameBuffer = (char *) (f->buf+sizeof(DIR));
  char    *dirBuffer  = (char *) (f->buf+sizeof(DIR)+34);
  FILINFO *fileInfo   = &m_fatFsFileInfo;

  if( m_dirBufferPtr>=m_dirBufferLen && (nameBuffer[0]!=0 || dir->obj.fs!=NULL) )
    {
      // nameBuffer[0]!=0  if we have not yet STARTED to read the directory
      // dir->obj.fs!=NULL if we have not yet FINISHED to read the directory
      m_dirBufferLen = 0;
      m_dirBufferPtr = 0;

      if( nameBuffer[0]!=0 )
        {
          m_ferror = f_findfirst(dir, fileInfo, getDriveSpec(nameBuffer[0]-'0'), nameBuffer+1);
          nameBuffer[0] = 0;
        }
      else
        m_ferror = f_findnext(dir, fileInfo);

      if( m_ferror != FR_OK )
        f_closedir(dir);
      else if( fileInfo->fname[0]==0 )
        {
          DWORD free;
          FATFS *fs = dir->obj.fs;
          f_getfree (getDriveSpec(fs->pdrv), &free, &fs);
          free = free*fs->csize*2;
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = 1;
          dirBuffer[m_dirBufferLen++] = free&255;
          dirBuffer[m_dirBufferLen++] = free/256;
          strcpy_P(dirBuffer+m_dirBufferLen, PSTR("BLOCKS FREE.             "));
          m_dirBufferLen += strlen(dirBuffer+m_dirBufferLen)+1;
          dirBuffer[m_dirBufferLen++] = 0;
          dirBuffer[m_dirBufferLen++] = 0;
          f_closedir(dir);
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


void IECFDC::openRawDir(FIL *f, const char *name)
{
  DIR     *dir       = (DIR  *)  f->buf;
  char    *dirBuffer = (char *) (f->buf+sizeof(DIR));

#if DEBUG>0
  Serial.println(F("OPEN-RAWDIR"));
#endif

  m_dirBufferPtr = 0;
  m_dirBufferLen = 0;
            
  byte drive = m_curDrive;
  if( name[1]=='0' || name[1]=='1' ) drive = name[1]-'0';

  startDiskOp();
  m_ferror = f_opendir(dir, getDriveSpec(drive));
  if( m_ferror == FR_OK )
    {
      // fake BAM
      memset(dirBuffer, 0, 254);
      dirBuffer[0] = 'A';
      f_getlabel(getDriveSpec(drive), dirBuffer+142, NULL); // label is 11 characters max
      int n = 142+strlen(dirBuffer+142);
      while( n<160 ) dirBuffer[n++] = 0xA0;
      dirBuffer[160] = '0';
      dirBuffer[161] = '0';
      dirBuffer[162] = 0xA0;
      dirBuffer[163] = 0x32;
      dirBuffer[164] = 'A';
      dirBuffer[165] = 0xA0;
      dirBuffer[166] = 0xA0;
      dirBuffer[167] = 0xA0;
      dirBuffer[168] = 0xA0;
      m_dirBufferLen = 254;
    }
}


bool IECFDC::readRawDir(FIL *f, byte *data)
{
  DIR     *dir       = (DIR *)      f->buf;
  char    *dirBuffer = (char    *) (f->buf+sizeof(DIR));
  FILINFO *fileInfo  = &m_fatFsFileInfo;

  if( m_dirBufferPtr>=m_dirBufferLen && dir->obj.fs!=NULL )
    {
      // fake raw directory entry
      m_dirBufferLen = 0;
      m_dirBufferPtr = 0;

      while( m_dirBufferLen+32 <= 256 )
        {
          m_ferror = f_readdir(dir, fileInfo);
          if( m_ferror!=FR_OK || fileInfo->fname[0]==0 )
            {
              f_closedir(dir);
              break;
            }
          else
            {
              uint16_t size = fileInfo->fsize==0 ? 0 : min(fileInfo->fsize/254+1, 65535);
              char *b = dirBuffer+m_dirBufferLen;
              memset(b, 0, 32);
              b[0] = 0x82; // program file
              memset(b+3, 0xA0, 16);
              memcpy(b+3, fileInfo->fname, strlen(fileInfo->fname)); // file name is 12 characters max ("12345678.EXT")
              b[28] = size&255;
              b[29] = size/256;
              m_dirBufferLen += 32;
            }
        }

      if( m_dirBufferLen>0 && m_dirBufferLen<256 )
        {
          // each sector must be 256 bytes => pad with 0 if we run out of entries
          memset(dirBuffer+m_dirBufferLen, 0, 256-m_dirBufferLen);
          m_dirBufferLen = 256;
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


void IECFDC::openFile(FIL *f, byte channel, const char *name)
{
#if DEBUG>0
  Serial.println(F("OPEN-FILE"));
#endif

  byte mode  = FA_READ;
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

          m_ferror = f_open(f, name, mode);
        }
      else
        {
          m_ferror = f_open(f, name, mode);
          if( m_ferror != FR_OK )
            {
              char *fname = findFile(f, (char *) name);
              if( m_ferror == FR_OK )
                m_ferror = f_open(f, fname, FA_READ);
            }
        }
    }

#if DEBUG>0
  if( m_ferror == FR_OK )
    Serial.println(F("=> OK"));
  else
    { Serial.print(F("=> FAIL:")); Serial.println(m_ferror); }
#endif
}


void IECFDC::open(byte channel, const char *name)
{
  byte fileIdx = 0;
  m_ferror = FR_OK;

  if( m_channelFiles[channel]<0xFF )
    m_ferror = FR_NO_CHANNEL;
  else
    {
      fileIdx = getAvailableFileIdx();
      if( fileIdx==IECFDC_MAXFILES )
        m_ferror = FR_TOO_MANY_OPEN_FILES;
      else
        {
          FIL *f = &(m_fatFsFile[fileIdx]);

          if( name[0]!='$' )
            openFile(f, channel, name);
          else if( channel==0 )
            { openDir(f, name); fileIdx |= 0x80; }
          else
            { openRawDir(f, name); fileIdx |= 0x40; }

          if( m_ferror==FR_OK )
            {
              m_channelFiles[channel] = fileIdx;
              m_numOpenFiles++;
            }
        }
    }

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();
}


byte IECFDC::read(byte channel, byte *buffer, byte bufferSize)
{
  byte res = 0;
  FIL *f = m_channelFiles[channel]==0xFF ? NULL : &(m_fatFsFile[m_channelFiles[channel]&0x0F]);

  if( f!=NULL )
    {
      if( f->obj.fs!=0 )
        {
          UINT count;
          m_ferror = f_read(f, buffer, bufferSize, &count);
          
          if( m_ferror == FR_OK && count>0 )
            res = count;
          else
            f_close(f);
        }
      else if( m_channelFiles[channel] & 0x80 )
        res = readDir(f, buffer) ? 1 : 0;
      else if( m_channelFiles[channel] & 0x40 )
        res = readRawDir(f, buffer) ? 1 : 0;

      if( !res )
        {
          m_channelFiles[channel] = 0xFF;
          m_numOpenFiles--;
        }
    }

  return res;
}


bool IECFDC::write(byte channel, byte data)
{
  bool res = false;
  FIL *f = m_channelFiles[channel]==0xFF ? NULL : &(m_fatFsFile[m_channelFiles[channel] & 0x0F]);

  if( f!=NULL )
    {
      UINT count;
      m_ferror = f_write(f, &data, 1, &count);

      if( m_ferror == FR_OK )
        res = true;
      else
        {
          f_close(f);
          m_channelFiles[channel] = 0xFF;
          m_numOpenFiles--;
        }
    }

  return res;
}


void IECFDC::close(byte channel)
{
  FIL *f = m_channelFiles[channel]==0xFF ? NULL : &(m_fatFsFile[m_channelFiles[channel] & 0x0F]);

  if( f!=NULL )
    {
      if( f->obj.fs!=0 )
        m_ferror = f_close(f);
      else 
        {
          DIR *dir = (DIR *) f->buf;
          if( dir->obj.fs!=NULL ) f_closedir(dir);
        }

      m_numOpenFiles--;
      m_channelFiles[channel] = 0xFF;
    }
}


void IECFDC::execute(const char *command, byte len)
{
  byte drive = 0;
  m_ferror = FR_OK;

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  if( command[0]=='X' || command[0]=='E' )
    {
      m_ferror = FR_EXTSTAT;
      command++;

      if( command[0]=='U' )
        {
          byte unit = command[1] - '0';
          if( unit<2 && (command[2]==0 || (command[2]=='!' && command[3]==0)) )
            {
              m_curDrive = unit;
              f_chdrive(getDriveSpec(unit));
              if( command[2]=='!' ) EEPROM.write(1, unit);
            }
          else
            m_ferror = FR_INVALID_PARAMETER;
        }
      else if( command[0]=='T' )
        {
          if( (command[1]=='0' || command[1]=='1') && (command[2]==':' || command[2]=='=') )
            {
              drive = command[1]-'0';
              command += 3;
              
              byte i=0;
              for(i=0; driveTypes[i]!=NULL; i++)
                if( strcmp(command, driveTypes[i])==0 )
                  break;
              
              if( driveTypes[i]==NULL && command[0]>='0' && command[0]<='6' && command[1]==0 )
                i = command[0]-'0';
              
              if( driveTypes[i]!=NULL )
                {
                  ArduinoFDC.setDriveType(drive, (enum ArduinoFDCClass::DriveType) i);
                  EEPROM.write(2+drive, i);
                }
              else
                m_ferror = FR_INVALID_PARAMETER;
            }
          else
            m_ferror = FR_INVALID_PARAMETER;
        }
      else if( command[0]>='1' && command[0]<='9' )
        {
          const char *c = command;
          byte devnr = *c++ - '0';
          if( *c>='0' && *c<='9' ) devnr = devnr*10 + *c++ - '0';
          if( *c!=0 && (*c=='!' && *(c+1)!=0) ) devnr = 0;

          if( devnr>2 && devnr<16 )
            {
              if( *c=='!' ) EEPROM.write(0, devnr);
              IECFileDevice::begin(devnr);
            }
          else
            m_ferror = FR_INVALID_PARAMETER;              
        }
      else if( command[0]!=0 )
        m_ferror = FR_INVALID_PARAMETER;
    }
  else if( parseCommand("S", &command, &drive) && *command!=0 )
    {
      FIL *f = getAvailableFile();
      if( f!=NULL )
        {
          DIR *dir = (DIR *) f->buf;
          FILINFO *fileInfo = (FILINFO *) (f->buf+sizeof(DIR));

          startDiskOp();
          
          byte n = 0;
          m_ferror = f_findfirst(dir, fileInfo, getDriveSpec(drive), command);
          while( m_ferror==FR_OK && fileInfo->fname[0]!=0 )
            {
              m_ferror = f_unlink(prefixDriveSpec(drive, fileInfo->fname));
              if( m_ferror==FR_OK )
                { n++; m_ferror = f_findnext(dir, fileInfo); }
            }
          
          f_closedir(dir);
          if( m_ferror==FR_OK ) { m_ferror = FR_SCRATCHED; m_errorTrack = n; }
        }
      else
        m_ferror = FR_TOO_MANY_OPEN_FILES;
    }
  else if( parseCommand("R", &command, &drive) && *command!=0 )
    {
      startDiskOp();

      char *equals = strchr(command, '=');
      if( equals==NULL )
        {
          // no "=" => rename disk
          m_ferror = f_setlabel(prefixDriveSpec(drive, command, 11));
        }
      else
        {
          *equals=0;
          if( equals!=command && *(equals-1)=='/' )
            {
              // new name ends in "/" => move to directory
              char dst[65];
              strcpy(dst, command);  // directory name
              strcat(dst, equals+1); // dst file name same as src file name
              m_ferror = f_rename(prefixDriveSpec(drive, equals+1), dst);
            }
          else
            {
              // regular rename
              m_ferror = f_rename(prefixDriveSpec(drive, equals+1), command);
            }
        }
    }
  else if( parseCommand("C", &command, &drive) && *command!=0 )
    {
      char *equals = strchr(command, '=');
      if( equals==NULL )
        m_ferror = FR_INVALID_PARAMETER;
      else
        {
          startDiskOp();

          *equals = 0;
          FIL *f1 = getAvailableFile();
          m_ferror = f1 ? f_open(f1, prefixDriveSpec(drive, command), FA_WRITE | FA_CREATE_NEW) : FR_TOO_MANY_OPEN_FILES;
          if( m_ferror==FR_OK )
            {
              command = equals+1;
              drive = m_curDrive;
              if( (command[0]=='0' || command[0]=='1') && (command[1]==':') )
                { drive = command[0]-'0'; command += 2; }

              if( *command!=0 )
                {
                  UINT rcount, wcount;
                  FIL *f2 = getAvailableFile();
                  m_ferror = f2 ? f_open(f2, prefixDriveSpec(drive, command), FA_READ) : FR_TOO_MANY_OPEN_FILES;
                  if( m_ferror==FR_OK )
                    {
                      byte buffer[64];
                      rcount = 1;
                      while( m_ferror==FR_OK && rcount>0 )
                        {
                          m_ferror = f_read(f2, buffer, 64, &rcount);
                          if( m_ferror==FR_OK && rcount>0 )
                            m_ferror = f_write(f1, buffer, rcount, &wcount);
                        }
                      
                      f_close(f2);
                    }
                }
              else
                m_ferror = FR_INVALID_PARAMETER;
                    
              f_close(f1);
            }
        }
    }
  else if( parseCommand("I", &command, &drive) )
    {
      startDiskOp();
      m_ferror = f_mount(&(m_fatFs[drive]), getDriveSpec(drive), 1);
    }
  else if( parseCommand("N", &command, &drive) && *command!=0 )
    {
      startDiskOp();
      // default interleave of 7 determined experimentally for 
      // maximum load performance with JiffyDOS on a 3.5" HD disk
      byte interleave = 7;
      char *comma = strchr(command, ',');
      if( comma!=NULL ) 
        {
          *comma = 0;
          char *comma2 = strchr(comma+1, ',');
          if( comma2!=NULL && *(comma2+1)>='1' && *(comma2+1)<='9' )
            interleave = *(comma2+1) - '0';
        }
      format(drive, command, comma!=NULL, interleave);
    }
  else if( parseCommand("MD", &command, &drive) && *command!=0 )
    {
      startDiskOp();
      m_ferror = f_mkdir(prefixDriveSpec(drive, command));
    }
  else if( parseCommand("RD", &command, &drive) && *command!=0 )
    {
      startDiskOp();
      m_ferror = f_rmdir(prefixDriveSpec(drive, command));
    }
  else if( parseCommand("CD", &command, &drive) && *command!=0 )
    m_ferror = f_chdir(prefixDriveSpec(drive, command));
  else if( strcmp_P(command, PSTR("CD_"))==0 )
    m_ferror = f_chdir("..");
  else if( command[0]=='U' )
    {
      if( command[1]==':' || command[1]=='J' )
        reset();
    }
  else if( strncmp(command, "M-W", 3)==0 )
    {
      command+=3; len-=3; 
      if( *command==':' ) { command++; len--; }
      if( len>=3 )
        {
          word addr = ((byte) command[0]) + (((byte) command[1])<<8);
          len  = min(len-3, command[2]);
          command+=3;
#if DEBUG>0
          Serial.print(F("MEMWRITE ")); Serial.print(addr, HEX); Serial.write(':'); 
          for(byte i=0;i<len; i++) { Serial.write(' '); Serial.print(command[i], HEX); }
          Serial.println();
#endif      
          if( addr<=119 && addr+len>120 && (command[119-addr]&0x0F)==(command[120-addr]&0x0F) )
            {
              byte newaddr = command[119-addr]&0x0F;
              IECFileDevice::begin(command[119-addr]&0x0F);
            }
          else
            m_ferror = FR_MEMOP; // general memory write not supported
        }
      else
        m_ferror = FR_INVALID_PARAMETER;
    }
  else if( strncmp(command, "M-R", 3)==0 )
    {
      command+=3; len-=3; 
      if( *command==':' ) { command++; len--; }
      if( len>=2 )
        {
          word addr = ((byte) command[0]) + (((byte) command[1])<<8);
          byte len  = len<3 ? 1 : command[2];
#if DEBUG>0
          Serial.print(F("MEMREAD ")); Serial.print(addr); Serial.write(':'); Serial.println(len, HEX); 
#endif
          if( addr==0xFFFF && len==1 )
            {
              // identify as C1541
              byte data[2] = {254, 0};
              setStatus((char *) data, 2);
            }
          else
            m_ferror = FR_MEMOP; // general memory read not supported
        }
      else
        m_ferror = FR_INVALID_PARAMETER;
    }
  else if( strncmp(command, "M-E", 3)==0 )
    m_ferror = FR_MEMOP;
  else
    m_ferror = FR_INVALID_PARAMETER;
}


void IECFDC::getStatus(char *buffer, byte bufferSize)
{
  byte code = 0;
  const char *message = NULL;
  switch( m_ferror )
    {
    case FR_OK:                  { code = 0;  message = PSTR(" OK"); break; }
    case FR_SCRATCHED:           { code = 1;  message = PSTR("FILES SCRATCHED"); break; }
    case FR_EXTSTAT:             { code = 2;  message = PSTR(""); break; }
    case FR_NOT_READY:
    case FR_NO_FILESYSTEM:
    case FR_DISK_ERR:            { code = 74; message = PSTR("DRIVE NOT READY"); m_errorTrack = m_ferror; break; }
    case FR_NO_PATH:
    case FR_NO_FILE:             { code = 62; message = PSTR("FILE NOT FOUND"); m_errorTrack = m_ferror; break; }
    case FR_EXIST:               { code = 63; message = PSTR("FILE EXISTS"); break; }
    case FR_WRITE_PROTECTED:     { code = 26; message = PSTR("WRITE PROTECT ON"); break; }
    case FR_DENIED:              { code = 81; message = PSTR("PERMISSION DENIED"); break; }
    case FR_INVALID_PARAMETER:   { code = 33; message = PSTR("SYNTAX ERROR"); break; }
    case FR_MEMOP:               { code = 93; message = PSTR("MEMOPS NOT SUPPORTED"); break; }
    case FR_NO_CHANNEL:          { code = 70; message = PSTR("NO CHANNEL"); break; }
    case FR_NOT_ENOUGH_CORE:     { code = 95; message = PSTR("OUT OF MEMORY"); break; }
    case FR_TOO_MANY_OPEN_FILES: { code = 96; message = PSTR("TOO MANY OPEN FILES"); break; }
    case FR_INVALID_NAME:        { code = 97; message = PSTR("INVALID NAME"); break; }
    case FR_INVALID_DRIVE:       { code = 98; message = PSTR("INVALID DRIVE"); break; }
    case FR_SPLASH:              { code = 73; message = PSTR("IECFDC-MEGA V1.0"); break; }
    default:                     { code = 99; message = PSTR("INTERNAL ERROR"); m_errorTrack = m_ferror; break; }
    }

  if( code<2 || code==73 ) m_errorSector = 0;
  byte i = 0;
  buffer[i++] = '0' + (code / 10);
  buffer[i++] = '0' + (code % 10);
  buffer[i++] = ',';

  if( code==2 )
    sprintf(buffer+i, 
            "U=%i:T0=%s:T1=%s", 
            m_curDrive, driveTypes[ArduinoFDC.getDriveType(0)], driveTypes[ArduinoFDC.getDriveType(1)]);
  else
    strncpy_P(buffer+i, message, bufferSize-11);

  i = strlen(buffer);
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

  for(byte i=0; i<IECFDC_MAXFILES; i++)
    if( m_fatFsFile[i].obj.fs!=0 ) 
      f_close(&(m_fatFsFile[i]));

  for(byte i=0; i<16; i++)
    m_channelFiles[i] = 0xFF;

  m_errorTrack = 0;
  m_errorSector = 0;
  m_numOpenFiles = 0;
  m_dirBufferLen = 0;
  m_dirBufferPtr = 0;
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, LOW);
  if( m_ferror==FR_OK ) m_ferror = FR_SPLASH;

  IECFileDevice::begin(EEPROM.read(0));  
}
