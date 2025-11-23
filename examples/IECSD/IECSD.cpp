// -----------------------------------------------------------------------------
// Copyright (C) 2025 David Hansel
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

#include "IECSD.h"

#define E_OK          0
#define E_SCRATCHED   1
#define E_READ       20
#define E_WRITE      28
#define E_INVCMD     31
#define E_INVNAME    33
#define E_NOTFOUND   62
#define E_EXISTS     63
#define E_MISMATCH   64
#define E_SPLASH     73
#define E_NOTREADY   74
#define E_MEMEXE     97
#define E_TOOMANY    98
#define E_VDRIVE     255

#define FT_NONE      0x00
#define FT_PRG       0x01
#define FT_SEQ       0x02
#define FT_DIR       0x04
#define FT_ANY       0xFF
#define FT_NODIR     ((FT_ANY) & ~(FT_DIR))

#define SHOW_LOWERCASE 0

#if !defined(SD_FAT_VERSION) || SD_FAT_VERSION<20200
#error This code requires SdFat library version 2.2.0 or later
#endif

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

// ----------------------------------------------------------------------------------------------

#ifdef HAVE_VDRIVE
static bool isDiskImage(const char *name)
{
  const char *dot = strrchr(name, '.');
  return dot!=NULL && (strcasecmp(dot, ".d64")==0 || strcasecmp(dot, ".g64")==0 || strcasecmp(dot, ".d71")==0 || strcasecmp(dot, ".d81")==0);
}
#endif


IECSD::IECSD(uint8_t devnum, uint8_t pinChipSelect, uint8_t pinLED) :
  IECFileDevice(devnum)
{
  m_cardOk = false;
  m_pinLED = pinLED;
  m_pinChipSelect = pinChipSelect;
  m_dirFormat = 1;
  m_suppressMemExeError = false;
  m_suppressReset  = false;
#ifdef HAVE_VDRIVE
  m_drive = NULL;
#if defined(PIN_BUTTON_IMAGE_PREV) && defined(PIN_BUTTON_IMAGE_NEXT)
  pinMode(PIN_BUTTON_IMAGE_PREV, INPUT_PULLUP);
  pinMode(PIN_BUTTON_IMAGE_NEXT, INPUT_PULLUP);
#endif
#endif
  m_cwd[IECSD_MAX_PATH] = 0;
  strcpy(m_cwd, "/");
}


void IECSD::begin()
{
  if( m_pinChipSelect<0xFF ) pinMode(m_pinChipSelect, OUTPUT);

  unsigned long ledTestEnd = millis() + 500;
  if( m_pinLED<0xFF ) { pinMode(m_pinLED, OUTPUT); digitalWrite(m_pinLED, HIGH); }

  m_errorCode = E_SPLASH;
  if( !checkCard() ) m_errorCode = E_NOTREADY;

  if( m_pinLED<0xFF ) 
    {
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      digitalWrite(m_pinLED, LOW); 
    }

  IECFileDevice::begin();
}


bool IECSD::checkCard()
{
  if( !m_cardOk ) 
#if defined(__SAM3X8E__)
    m_cardOk = m_sd.begin(m_pinChipSelect, SD_SCK_MHZ(2));
#elif defined(ARDUINO_ARCH_ESP32)
    m_cardOk = m_sd.begin(m_pinChipSelect, SD_SCK_MHZ(8));
#else
    m_cardOk = m_sd.begin(m_pinChipSelect);
#endif

  return m_cardOk;
}


void IECSD::task()
{
  // handle status LED
  if( m_pinLED<0xFF )
    {
      static unsigned long nextblink = 0;
      if( m_errorCode==E_OK || m_errorCode==E_SPLASH || m_errorCode==E_SCRATCHED )
        {
          bool active = m_dir || m_file;
#ifdef HAVE_VDRIVE
          active |= (m_drive!=NULL) && (m_drive->getNumOpenChannels()>0);
#endif
          digitalWrite(m_pinLED, active);
        }
      else if( millis()>nextblink )
        {
          digitalWrite(m_pinLED, !digitalRead(m_pinLED));
          nextblink += (m_errorCode==E_MEMEXE) ? 125 : 500;
        }
    }

#if defined(HAVE_VDRIVE) && defined(PIN_BUTTON_IMAGE_NEXT) && defined(PIN_BUTTON_IMAGE_PREV)
  if( !digitalRead(PIN_BUTTON_IMAGE_NEXT) || !digitalRead(PIN_BUTTON_IMAGE_PREV) )
    {
      bool findPrev = !digitalRead(PIN_BUTTON_IMAGE_PREV);
      const char *imgName = m_drive==NULL ? NULL : m_drive->getDiskImageFilename();
      if( imgName && strrchr(imgName, '/')!=NULL ) imgName = strrchr(imgName, '/')+1;

      bool found = false;
      SdFile file, dir;
      if( dir.openCwd() )
        {
          char buf1[256], buf2[256];
          if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);

          buf2[0] = 0;
          while( !found && file.openNext(&dir, O_RDONLY) )
            {
              file.getName(buf1, 256);
              buf1[255]=0;
              found = (imgName==NULL || strcmp(buf1, imgName)==0);
              if( !found && isDiskImage(buf1) )
                strcpy(buf2, buf1);
            }

          if( found )
            {
              buf1[0] = 0;
              while( buf1[0]==0 && file.openNext(&dir, O_RDONLY) )
                {
                  file.getName(buf1, 256);
                  if( !isDiskImage(buf1) ) buf1[0] = 0;
                }

              if( findPrev && *buf1 )
                {
                  delete m_drive;
                  m_drive= VDrive::create(0, buf1);
                }
              else if( !findPrev && *buf2 )
                {
                  delete m_drive;
                  m_drive= VDrive::create(0, buf2);
                }
            }

          if( m_pinLED<0xFF ) digitalWrite(m_pinLED, LOW);
        }

      while( !digitalRead(PIN_BUTTON_IMAGE_NEXT) || !digitalRead(PIN_BUTTON_IMAGE_PREV) );
      delay(50);
    }
#endif

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();
}


void IECSD::toPETSCII(uint8_t *name)
{
  while( *name )
    {
      if( *name>=65 && *name<=90 && SHOW_LOWERCASE )
        *name += 32;
      else if( *name>=97 && *name<=122 )
        *name -= 32;

      name++;
    }
}


void IECSD::fromPETSCII(uint8_t *name)
{
  while( *name )
    {
      if( *name==0xFF )
        *name = '~';
      else if( *name>=192 ) 
        *name -= 96;

      if( *name>=65 && *name<=90 )
        *name += 32;
      else if( *name>=97 && *name<=122 )
        *name -= 32;

      name++;
    }
}

#if defined(IEC_FP_EPYX) && defined(IEC_FP_EPYX_SECTOROPS) && defined(HAVE_VDRIVE)
bool IECSD::epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  if( m_drive!=NULL )
    res = m_drive->readSector(track, sector, buffer);

  // for debug log
  if( res ) IECFileDevice::epyxReadSector(track, sector, buffer);

  return res;
}


bool IECSD::epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  // for debug log
  IECFileDevice::epyxWriteSector(track, sector, buffer);

  if( m_drive!=NULL )
    res = m_drive->writeSector(track, sector, buffer);

  return res;
}
#endif


uint8_t IECSD::chdir(const char *c)
{
  char dir[IECSD_MAX_PATH+1];
  memcpy(dir, m_cwd, IECSD_MAX_PATH+1);
  fromPETSCII((uint8_t *) dir);

  // if this is an absolute path then start at the root
  if( c[0]=='/' || c[0]=='^' )
    {
#ifdef HAVE_VDRIVE
      if( m_drive!=NULL ) { delete m_drive; m_drive=NULL; }
#endif
      strcpy(dir, "/");
      c++;
    }

  uint8_t res = E_OK;
  while( (res==E_OK) && c!=NULL && *c!=0 )
    {
      const char *nextslash = strchr(c, '/');
      if( nextslash==0 ) nextslash = c+strlen(c);
      int len = nextslash-c;
      int dirlen = strlen(dir);

      if( (len==2 && c[0]=='.' && c[1]=='.') || (len==1 && c[0]=='_') )
        {
#ifdef HAVE_VDRIVE
          if( m_drive!=NULL ) 
            { delete m_drive; m_drive=NULL; }
          else
#endif
            {
              char *lastarc = strrchr(dir, '/');
              if( lastarc==dir ) lastarc++;
              if( lastarc!=NULL ) *lastarc = 0;
            }
        }
      else if( len==1 && c[0]=='.' )
        {
          // "." is current directory, nothing to do
        }
      else if( len==1 && c[0]=='^' )
        {
          // "up arrow" means top-level directory
          strcpy(dir, "/");
        }
      else if( len>0 && dirlen+len+1<IECSD_MAX_PATH )
        {
          // dir should be a valid directory at this point
          m_sd.chdir(dir);

          // append the new arc to the directory
          if( dirlen>0 && dir[dirlen-1]!='/') strcat(dir, "/");
          strncat(dir, c, len);

          // get a pointer to the beginning of the new arc
          char *newarc = strrchr(dir, '/')+1;

          SdFile f;
          if( !f.open(newarc, O_RDONLY) )
            {
              // "dir" is not a valid file or directory
              // try to find the last arc with any extension (and wildcards)
              const char *fn = findFile(newarc, FT_ANY);
              if( fn!=NULL && dirlen+strlen(fn)+1<IECSD_MAX_PATH )
                {
                  // found a match, copy the full matching file name over the new arc
                  strcpy(newarc, fn);
                  f.open(fn, O_RDONLY);
                }
            }

          if( !f )
            {
              // can't find the new arc => fail
              res = E_NOTFOUND;
            }
#ifdef HAVE_VDRIVE
          else if( !f.isDir() && (m_drive=VDrive::create(0, dir))!=NULL )
            {
              // dir is a disk image, set cwd to directory containing the image
              char *lastarc = strrchr(dir, '/');
              if( lastarc==dir ) lastarc++;
              if( lastarc!=NULL ) *lastarc = 0;
              strcpy(m_cwd, dir);

              // return immediately, ignore rest of path
              return E_OK;
            }
#endif
          else if( !f.isDir() )
            {
              // the new arc is not a directory
              res = E_MISMATCH;
            }
        }

      c = nextslash;
      if( *c=='/' ) c++;
    }

  // double-check that the result is a good directory
  if( (res==E_OK) && !m_sd.chdir(dir) )
    res = E_NOTFOUND;

  // if we succeeded then update cwd, otherwise go back to previous directory
  if( res==E_OK )
    strcpy(m_cwd, dir);
  else
    m_sd.chdir(m_cwd);

  return res;
}


uint8_t IECSD::openDir(const char *pattern)
{
  uint8_t res = E_OK;

  while( *pattern && isspace(*pattern) ) pattern++;

  if( pattern[0]==':' )
    pattern += 1;
  else if( isdigit(pattern[0]) )
    {
      if( pattern[1]==':' )
        pattern += 2;
      else
        pattern = "*";
    }

  m_dirPattern = NULL;
  if( pattern[0]!=0 && strlen(m_cwd) + strlen(pattern) + 2 < IECSD_MAX_PATH )
    {
      m_dirPattern = m_cwd + strlen(m_cwd) + 1;
      strcpy(m_dirPattern, pattern);
    }

  if( m_dir.openCwd() )
    {
      m_buffer[0] = 0x01;
      m_buffer[1] = 0x08;
      m_buffer[2] = 1;
      m_buffer[3] = 1;
      m_buffer[4] = 0;
      m_buffer[5] = 0;
      m_buffer[6] = 18;
      m_buffer[7] = '"';
      size_t n = m_dir.getName(m_buffer+8, 16);
      toPETSCII((uint8_t *) m_buffer+8);
      while( n<16 ) { m_buffer[8+n] = ' '; n++; }
      strcpy_P(m_buffer+24, PSTR("\" 00 2A"));
      m_bufferLen = 32;
      m_bufferPtr = 0;
      if( strcmp(m_cwd, "/")!=0 )
        {
          memcpy_P(m_buffer+m_bufferLen, PSTR("\x01\x01\x00\x00   \"..\"               DIR  "), 32);
          m_bufferLen += 32;
        }

      res = E_OK;
    }
  else
    res = E_NOTREADY;
  
  return res;
}


bool IECSD::readDir(uint8_t *data)
{
  if( m_bufferPtr==m_bufferLen && m_dir )
    {
      bool repeat = true;
      while( repeat )
        {
          m_bufferPtr = 0;
          m_bufferLen = 0;

          SdFile f;
          if( f.openNext(&m_dir, O_RDONLY) )
            {
              uint16_t size = f.fileSize()==0 ? 0 : min(f.fileSize()/254+1, 9999);
              m_buffer[m_bufferLen++] = 1;
              m_buffer[m_bufferLen++] = 1;
              m_buffer[m_bufferLen++] = size&255;
              m_buffer[m_bufferLen++] = size/256;
              if( size<10 )    m_buffer[m_bufferLen++] = ' ';
              if( size<100 )   m_buffer[m_bufferLen++] = ' ';
              if( size<1000 )  m_buffer[m_bufferLen++] = ' ';

              uint8_t maxNameLen = m_dirFormat==2 ? IECSD_BUFSIZE-18 : 16;

              m_buffer[m_bufferLen++] = '"';
              size_t n = f.getName(m_buffer+m_bufferLen, maxNameLen+4);
#if SDFAT_FILE_TYPE == 3
              if( (f.attrib() & (FS_ATTRIB_SYSTEM | FS_ATTRIB_HIDDEN))!=0 ) 
                n = 0;
              else if( n==0 )
                {
                  char buf[128];
                  n = f.getName(buf, 128);
                  if( n>0 ) { strncpy(m_buffer+m_bufferLen, buf, maxNameLen+4); n = maxNameLen+4; }
                }
#else
              if( f.isSystem() ) 
                n = 0;
              else if( n==0 ) 
                n = f.getSFN(m_buffer+m_bufferLen, maxNameLen+4);
#endif
              if( n>0 )
                {
                  toPETSCII((uint8_t *) m_buffer+m_bufferLen);
                  if( m_dirPattern && !isMatch(m_buffer+m_bufferLen, m_dirPattern, FT_ANY) )
                    n = 0;
                }

              if( n>0 )
                {
                  char ftype[3] = {'P', 'R', 'G'};
                  char *dot = strrchr(m_buffer+m_bufferLen, '.');
                  if( dot!=NULL && isalnum(dot[1]) && isalnum(dot[2]) && isalnum(dot[3]) && dot[4]==0 )
                    {
                      ftype[0] = dot[1];
                      ftype[1] = dot[2];
                      ftype[2] = dot[3];
                      if( m_dirFormat==0 ) n = dot-(m_buffer+m_bufferLen);
                    }
                  
                  // if the actual file name is longer than what we can show, use "*" 
                  // as the last shown character, that way LOADing from the directory
                  // listing works
                  if( n>maxNameLen ) { n = maxNameLen; m_buffer[m_bufferLen+n-1]='*'; }

                  // If the user LOADs directly from the directory listing
                  // a "," in the listing would be interpreted as the start of file type 
                  // ("P", "S") and/or mode ("R", "W") => replace with "?"
                  for(uint8_t i=0; i<n; i++) 
                    if( m_buffer[m_bufferLen+i]==',' )
                      m_buffer[m_bufferLen+i]='?';

                  m_bufferLen += n;
                  m_buffer[m_bufferLen++] = '"';
                  m_buffer[m_bufferLen++] = ' ';
                  while( n++<16 ) m_buffer[m_bufferLen++] = ' ';
                  memcpy(m_buffer+m_bufferLen, ftype, 3);
                  m_bufferLen += 3;
                  while( m_bufferLen<31 ) m_buffer[m_bufferLen++] = ' ';
                  m_buffer[m_bufferLen++] = 0;
                  repeat = false;
                }

              f.close();
            }
          else
            {
              uint32_t free = min(65535, m_sd.bytesPerCluster() * m_sd.clusterCount() / 254);
              m_buffer[0] = 1;
              m_buffer[1] = 1;
              m_buffer[2] = free&255;
              m_buffer[3] = free/256;
              strcpy_P(m_buffer+4, PSTR("BLOCKS FREE.             "));
              m_buffer[29] = 0;
              m_buffer[30] = 0;
              m_buffer[31] = 0;
              m_bufferLen = 32;
              m_dir.close();
              repeat = false;
            }
        }
    } 
      
  if( m_bufferPtr<m_bufferLen )
    {
      *data = m_buffer[m_bufferPtr++];
      return true;
    }
  else
    return false;
}


bool IECSD::isMatch(const char *name, const char *pattern, uint8_t ftypes)
{
  signed char found = -1;

  for(uint8_t i=0; found<0; i++)
    {
      if( pattern[i]=='*' )
        found = 1;
      else if( pattern[i]==0 && name[i]=='.' )
        {
          if( ftypes==FT_ANY )
            found = 1;
          else if( (ftypes & FT_PRG) && strcasecmp_P(name+i+1, PSTR("prg"))==0 )
            found = 1;
          else if( (ftypes & FT_SEQ) && strcasecmp_P(name+i+1, PSTR("seq"))==0 )
            found = 1;
          else
            found = 0;
        }
      else if( pattern[i]==0 || name[i]==0 )
        found = (pattern[i]==name[i]) ? 1 : 0;
      else if( pattern[i]!='?' && tolower(pattern[i])!=tolower(name[i]) && !(name[i]=='~' && (pattern[i] & 0xFF)==0xFF) )
        found = 0;
    }

  return found==1;
}


const char *IECSD::findFile(const char *pattern, uint8_t ftypes)
{
  bool found = false;

  if( m_dir.openCwd() )
    {
      m_file.close();
      while( !found && m_file.openNext(&m_dir, O_RDONLY) )
        {
          found = ((ftypes&FT_DIR)!=0 || !m_file.isDir()) && m_file.getName(m_buffer, IECSD_BUFSIZE) && isMatch(m_buffer, pattern, ftypes);
#if SDFAT_FILE_TYPE != 3
          if( !found )
            found = ((ftypes&FT_DIR)!=0 || !m_file.isDir()) && m_file.getSFN(m_buffer, IECSD_BUFSIZE) && isMatch(m_buffer, pattern, ftypes);
#endif
          m_file.close();
        }
      
      m_dir.close();
    }

  return found ? m_buffer : NULL;
}


uint8_t IECSD::openFile(uint8_t channel, const char *constName)
{
  uint8_t res = E_OK;
  char ftype = FT_PRG;
  char mode  = 'R';
  char namebuf[41];
  char *name = namebuf;

  strncpy(name, constName, 40);
  name[40]=0;
  char *c = strchr(name, '\xa0');
  if( c!=NULL ) *c = 0;
  fromPETSCII((uint8_t *) name);

  char *comma = strchr(name, ',');
  if( comma!=NULL )
    {
      char *c = comma;
      do { *c-- = 0; } while( c!=name && ((*c) & 0x7f)==' ');
      char cc = toupper(*(comma+1));
      if( cc=='R' || cc=='W' )
        mode = cc;
      else
        {
          if( cc=='S' )
            ftype = FT_SEQ;
          else if( cc!='P' )
            ftype = FT_NONE;

          comma = strchr(comma+1, ',');
          if( comma!=NULL )
            mode = toupper(*(comma+1));
        }
    }
  else if( channel==0 )
    mode = 'R';
  else if( channel==1 )
    mode = 'W';
  
  if( (ftype!=FT_PRG && ftype!=FT_SEQ) || (mode!='R' && mode!='W') )
    res = E_INVNAME;

  if( res == E_OK )
    {
      if( mode=='R' )
        {
          if( name[0]==':' ) 
            name++;
          else if( name[0]!=0 && name[1]==':' )
            name+=2;
          
          // first try to find the file with its given type (and wildcards)
          // if that fails then try again with any extension
          const char *fn = findFile(name, ftype|FT_DIR);
          if( fn==NULL && m_dirFormat==0 ) fn = findFile(name, FT_ANY);

          if( fn!=NULL )
            {
              m_file.open(fn, O_RDONLY);
#if SDFAT_FILE_TYPE == 1
              if( !m_file.isOpen() ) m_file.openExistingSFN(fn);
#endif
              if( m_file.isOpen() )
                {
                  if( m_file.isDir() )
                    {
                      // the file is a directory => cd into it and open it
                      m_file.close();
                      if( channel==0 )
                        {
                          chdir(name);
                          res = openDir("*");
                        }
                      else
                        res = E_MISMATCH;
                    }
#ifdef HAVE_VDRIVE
                  else if( (m_drive=VDrive::create(0, fn))!=NULL )
                    {
                      // the file is a mountable disk image => mount it and open its directory
                      m_file.close();
                      if( channel==0 )
                        res = m_drive->openFile(channel, "$") ? E_OK : E_VDRIVE;
                      else
                        res = E_MISMATCH;
                    }
#endif
                }
              else
                res = E_NOTFOUND;
            }
          else if( strcmp(name, "..")==0 )
            {
              chdir(name);
              res = openDir("*");
            }
          else
            res = E_NOTFOUND;

          // reject file if size is 0
          if( m_file.isOpen() && (m_file.fileSize()==0) ) res = E_NOTFOUND;
          if( res != E_OK ) m_file.close();
        }
      else
        {
          bool overwrite = false;
          if( name[0]=='@' && name[1]==':' )
            { name+=2; overwrite = true; }
          else if( name[0]=='@' && name[1]!=0 && name[2]==':' )
            { name+=3; overwrite = true; }
          else if( name[0]!=0 && name[1]==':' )
            { name+=2; }

          // if we are overwriting a file whose name exists without PRG/SEQ extension 
          // then delete the existing version (so we don't get two files with the same name)
          if( overwrite && findFile(name, FT_NONE)!=NULL )
            {
              m_dir.openCwd();
              m_dir.remove(name);
              m_dir.close();
            }
          
          strcat_P(name, ftype==FT_PRG ? PSTR(".prg") : PSTR(".seq"));
          if( m_file.open(name, O_WRONLY | O_CREAT | (overwrite ? O_TRUNC : O_EXCL)) )
            res = E_OK;
          else
            {
              res = E_WRITE;
              if( !overwrite && m_dir.openCwd() )
                {
                  if( m_dir.exists(name) ) res = E_EXISTS;
                  m_dir.close();
                }
            }
        }
    }

  return res;
}


bool IECSD::open(uint8_t channel, const char *name, uint8_t nameLen)
{
  if( !checkCard() )
    m_errorCode = E_NOTREADY;
#ifdef HAVE_VDRIVE
  else if( m_drive!=NULL )
    m_errorCode = m_drive->openFile(channel, name, nameLen) ? E_OK : E_VDRIVE;
#endif
  else if( channel==0 && name[0]=='$' )
    m_errorCode = openDir(name+1);
  else if ( !m_file )
    m_errorCode = openFile(channel, name);
  else
    {
      // we can only have one file open at a time
      m_errorCode = E_TOOMANY;
    }

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  return m_errorCode==E_OK;
}


uint8_t IECSD::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
#ifdef HAVE_VDRIVE
  if( m_drive!=NULL && m_drive->isFileOk(channel) )
    {
      size_t n = bufferSize;
      if( !m_drive->read(channel, buffer, &n, eoi) )
        {
          m_errorCode = E_VDRIVE;
          return 0;
        }

      return n;
    }
  else 
#endif
  if( m_file.isOpen() )
    return m_file.read(buffer, bufferSize);
  else
    return readDir(buffer) ? 1 : 0;
}


uint8_t IECSD::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
#ifdef HAVE_VDRIVE
  if( m_drive!=NULL && m_drive->isFileOk(channel) )
    {
      size_t n = bufferSize;
      if( !m_drive->write(channel, buffer, &n) )
        {
          m_errorCode = E_VDRIVE;
          return 0;
        }

      return n;
    }
  else 
#endif
  if( m_file.isOpen() )
    return m_file.write(buffer, bufferSize);
  else
    return 0;
}


void IECSD::close(uint8_t channel)
{
  // 1541 drive clears status when closing a channel (needed in "Odell Lake" game)
  clearStatus();

#ifdef HAVE_VDRIVE
  if( m_drive!=NULL && m_drive->isFileOk(channel) )
    {
      m_drive->closeFile(channel);
    }
  else 
#endif
  if( m_dir )
    { 
      m_dir.close();
      m_bufferLen = 0;
    }
  else 
    m_file.close(); 
}


bool IECSD::isMemExeCommand(const char *command) const
{
  return (strncmp(command, "M-E", 3)==0 || strncmp(command, "B-E", 3)==0 ||
          (command[0]=='U' && command[1]>='3' && command[1]<='8') ||
          (command[0]=='U' && command[1]>='C' && command[1]<='H'));
}


void IECSD::executeData(const uint8_t *data, uint8_t len)
{
  // This function deals with executing commands that may contain binary data,
  // i.e. the command itself may contain a NUL character and/or may end in one
  // or more CRs ($13). Text-based commands are dealt with in execute(command) below.
  const char *command = (const char *) data;

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();
  digitalWrite(m_pinLED, HIGH);

#ifdef HAVE_VDRIVE
  if( m_drive!=NULL && strncmp_P(command, PSTR("CD"),2)!=0 && !isMemExeCommand(command) )
    {
      m_errorCode = m_drive->execute(command, len)==0 ? E_VDRIVE : E_OK;

      // when executing commands that read data into a buffer or reposition
      // the pointer we need to clear our read buffer of the channel for which
      // this command is issued, otherwise remaining characters in the buffer 
      // will be prefixed to the data from the new record or buffer location
      if( command[0]=='P' && len>=2 )
        clearReadBuffer(command[1] & 0x0f);
      else if( strncmp(command, "B-P", 3)==0 || strncmp(command, "B-R", 3)==0 )
        {
          int i = 3;
          while( i<len && !isdigit(command[i]) ) i++;
          if( i<len ) clearReadBuffer(atoi(command+i));
        }
      else if( strncmp(command, "U1", 2)==0 )
        {
          int i = 2;
          while( i<len && !isdigit(command[i]) ) i++;
          if( i<len ) clearReadBuffer(atoi(command+i));
        }
    }
#else
  if(0) {}
#endif
  else if( strncmp_P(command, PSTR("M-R\xfa\x02\x03"), 6)==0 )
    {
      // hack: DolphinDos' MultiDubTwo reads 02FA-02FC to determine
      // number of free blocks => pretend we have 664 (0298h) blocks available
      uint8_t data[3] = {0x98, 0, 0x02};
      setStatus((char *) data, 3);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-R"), 3)==0 && len>=5 )
    {
      // memory read not supported => always return 0xFF
      uint8_t n = min(len==5 ? 1 : command[5], IECSD_BUFSIZE);
      memset(m_buffer, 0xFF, n);
      setStatus(m_buffer, n);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-W"), 3)==0 )
    {
      // memory write not supported => ignore
      m_errorCode = E_OK;
    }
  else
    {
      // calling IECFileDevice::executeData will strip off trailing CRs, make sure the 
      // command is 0-terminated and then call IECSD::execute(command) below
      IECFileDevice::executeData(data, len);
    }

  digitalWrite(m_pinLED, LOW);
}


void IECSD::execute(const char *command)
{
  if( strncmp_P(command, PSTR("CD"),2)==0 )
    {
      // "CD" command: if there is a colon then ignore anything before (and including) the colon
      const char *colon = strchr(command, ':');
      m_errorCode = chdir(colon==NULL ? command+2 : colon+1);
    }
  else if( isMemExeCommand(command) )
    {
      // M-E and related commands not supported
      m_errorCode = m_suppressMemExeError ? E_OK : E_MEMEXE;

      // block and blink LED for 2 seconds - blinking will continue after
      // but this should ensure that the user sees the blinking before the
      // computer sends another command
      if( m_errorCode==E_MEMEXE && m_pinLED<0xFF )
        for(int i=0; i<2*8; i++)
          { digitalWrite(m_pinLED, !digitalRead(m_pinLED)); delay(125); }
    }
  else if( strncmp(command, "S:", 2)==0 )
    {
      if( m_dir.openCwd() )
        {
          char pattern[17];
          m_errorCode = E_SCRATCHED;
          m_scratched = 0;

          strncpy(pattern, command+2, 16);
          pattern[16]=0;
          fromPETSCII((uint8_t *) pattern);
          
          while( m_file.openNext(&m_dir, O_RDONLY) )
            {
              size_t n = m_file.getName(m_buffer, IECSD_BUFSIZE);
              m_file.close();
              if( n>0 && isMatch(m_buffer, pattern, FT_NODIR) && m_dir.remove(m_buffer) )
                m_scratched++;
            }
          
          m_dir.close();
        }
      else
        m_errorCode = E_NOTREADY;
    }
#ifdef HAVE_VDRIVE
  else if( strncmp(command, "N:", 2)==0 )
    {
      const char *imagename = command+2;
      const char *dot = strchr(imagename, '.');
      if( dot==NULL )
        m_errorCode = E_INVNAME;
      else
        {
          char diskname[17];
          memset(diskname, 0, 17);
          strncpy(diskname, imagename, (dot-imagename)<16 ? (dot-imagename) : 16);

          m_dir.openCwd();
          if( m_dir.exists(imagename) )
            m_errorCode = E_EXISTS;
          else if( VDrive::createDiskImage(imagename, dot+1, diskname, false) )
            m_errorCode = E_OK;
          else
            m_errorCode = E_INVNAME;
          m_dir.close();
        }
    }
#endif
  else if( (strncmp_P(command, PSTR("MD:"),3)==0 || strncmp_P(command, PSTR("RD:"),3)==0) && command[3]!=0 )
    {
      strncpy(m_buffer, command+3, IECSD_BUFSIZE);
      m_buffer[IECSD_BUFSIZE-1]=0;
      fromPETSCII((uint8_t *) m_buffer);

      if( command[0]=='M' )
        m_errorCode = m_sd.mkdir(m_buffer, true) ? E_OK : E_EXISTS;
      else if( command[0]=='R' )
        m_errorCode = m_sd.rmdir(m_buffer) ? E_OK : E_NOTFOUND;
    }
  else if( strcmp(command, "I")==0 || strcmp_P(command, PSTR("X+\x0dUJ"))==0 )
    {
      m_dir.close();
      m_file.close();
      m_errorCode = E_OK;
    }
  else if( command[0]=='X' || command[0]=='E' )
    {
      command++;
      if( command[0]>='1' && command[0]<='9' )
        {
          const char *c = command;
          uint8_t devnr = *c++ - '0';
          if( *c>='0' && *c<='9' ) devnr = devnr*10 + *c++ - '0';
          if( *c!=0 ) devnr = 0;

          if( devnr>2 && devnr<16 )
            {
              m_devnr = devnr;
              m_errorCode = E_OK;
            }
          else
            m_errorCode = E_INVCMD;
        }
      else if( command[0]=='D' && isdigit(command[1]) )
        m_dirFormat = command[1]-'0';
      else if( command[0]=='E' && isdigit(command[1]) )
        m_suppressMemExeError = command[1]=='0';
      else if( command[0]=='R' && isdigit(command[1]) )
        m_suppressReset = command[1]=='0';
      else
        m_errorCode = E_INVCMD;
    }
  else
    m_errorCode = E_INVCMD;
}


uint8_t IECSD::getStatusData(char *buffer, uint8_t bufferSize, bool *eoi)
{ 
#ifdef HAVE_VDRIVE
  // if we have an active VDrive then just return its status
  if( m_drive!=NULL && m_errorCode!=E_MEMEXE )
    {
      m_errorCode = E_OK;
      return m_drive->getStatusBuffer(buffer, bufferSize, eoi);
    }
#endif

  // IECFileDevice::getStatusData will in turn call IECSD::getStatus()
  return IECFileDevice::getStatusData(buffer, bufferSize, eoi);
}


void IECSD::getStatus(char *buffer, uint8_t bufferSize)
{
  const char *message = NULL;
  switch( m_errorCode )
    {
    case E_OK:                   { message = PSTR(" OK"); break; }
    case E_READ:                 { message = PSTR("READ ERROR"); break; }
    case E_WRITE:                { message = PSTR("WRITE ERROR"); break; }
    case E_SCRATCHED:            { message = PSTR("FILES SCRATCHED"); break; }
    case E_NOTREADY:             { message = PSTR("DRIVE NOT READY"); break; }
    case E_NOTFOUND:             { message = PSTR("FILE NOT FOUND"); break; }
    case E_EXISTS:               { message = PSTR("FILE EXISTS"); break; }
    case E_MISMATCH:             { message = PSTR("FILE TYPE MISMATCH"); break; }
    case E_INVCMD:
    case E_INVNAME:              { message = PSTR("SYNTAX ERROR"); break; }
    case E_MEMEXE:               { message = PSTR("M-E NOT SUPPORTED"); break; }
    case E_TOOMANY:              { message = PSTR("TOO MANY OPEN FILES"); break; }
    case E_SPLASH:               { message = PSTR("IECSD V1.0"); break; }
    default:                     { message = PSTR("UNKNOWN"); break; }
    }

  uint8_t i = 0;
  buffer[i++] = '0' + (m_errorCode / 10);
  buffer[i++] = '0' + (m_errorCode % 10);
  buffer[i++] = ',';
  strcpy_P(buffer+i, message);
  i += strlen_P(message);

  if( m_errorCode!=E_SCRATCHED ) m_scratched = 0;
  buffer[i++] = ',';
  buffer[i++] = '0' + (m_scratched / 10);
  buffer[i++] = '0' + (m_scratched % 10);
  strcpy_P(buffer+i, PSTR(",00\r"));

  m_errorCode = E_OK;
}


void IECSD::reset()
{
  if( !m_suppressReset )
    {
      unsigned long ledTestEnd = millis() + 250;
      if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);

      IECFileDevice::reset();

      m_cardOk = false;
      m_errorCode = E_SPLASH;
#ifdef HAVE_VDRIVE
      if( m_drive!=NULL )
        {
          m_drive->closeAllChannels();
          m_drive->execute("UJ", 2, false);
        }
#endif
      if( checkCard() )
        m_sd.chdir(m_cwd);
      else
        m_errorCode = E_NOTREADY;

      m_file.close();
      m_dir.close();

      if( m_pinLED<0xFF )
        {
          unsigned long t = millis();
          if( t<ledTestEnd ) delay(ledTestEnd-t);
          digitalWrite(m_pinLED, LOW);
        }
    }
}
