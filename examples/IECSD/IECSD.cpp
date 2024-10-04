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

#include "IECSD.h"

#define E_OK          0
#define E_SCRATCHED   1
#define E_READ       20
#define E_WRITE      28
#define E_INVCMD     31
#define E_INVNAME    33
#define E_NOTFOUND   62
#define E_EXISTS     63
#define E_SPLASH     73
#define E_NOTREADY   74
#define E_TOOMANY    98

#define SHOW_LOWERCASE 0

#if !defined(SD_FAT_VERSION) || SD_FAT_VERSION<20200
#error This code requires SdFat library version 2.2.0 or later
#endif

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif


IECSD::IECSD(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinChipSelect, byte pinLED) :
  IECFileDevice(pinATN, pinCLK, pinDATA, pinRESET, 0xFF)
{
  m_pinLED = pinLED;
  m_pinChipSelect = pinChipSelect;
}


void IECSD::begin(byte device)
{
  IECFileDevice::begin(device);
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
}


bool IECSD::checkCard()
{
  if( !m_cardOk ) 
#if defined(__SAM3X8E__) || defined(ARDUINO_ARCH_ESP32)
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
        digitalWrite(m_pinLED, m_dir || m_file);
      else if( millis()>nextblink )
        {
          digitalWrite(m_pinLED, !digitalRead(m_pinLED));
          nextblink += 500;
        }
    }

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();
}


void IECSD::toPETSCII(byte *name)
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


void IECSD::fromPETSCII(byte *name)
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

byte IECSD::openDir()
{
  byte res = E_OK;

  if( m_dir.openCwd() )
    {
      m_dirBuffer[0] = 0x01;
      m_dirBuffer[1] = 0x08;
      m_dirBuffer[2] = 1;
      m_dirBuffer[3] = 1;
      m_dirBuffer[4] = 0;
      m_dirBuffer[5] = 0;
      m_dirBuffer[6] = 18;
      m_dirBuffer[7] = '"';
      size_t n = m_dir.getName(m_dirBuffer+8, 16);
      toPETSCII((byte *) m_dirBuffer+8);
      while( n<16 ) { m_dirBuffer[8+n] = ' '; n++; }
      strcpy_P(m_dirBuffer+24, PSTR("\" 00 2A"));
      m_dirBufferLen = 32;
      m_dirBufferPtr = 0;
      res = E_OK;
    }
  else
    res = E_NOTREADY;
  
  return res;
}


bool IECSD::readDir(byte *data)
{
  if( m_dirBufferPtr==m_dirBufferLen && m_dir )
    {
      bool repeat = true;
      while( repeat )
        {
          m_dirBufferPtr = 0;
          m_dirBufferLen = 0;

          SdFile f;
          if( f.openNext(&m_dir, O_RDONLY) )
            {
              uint16_t size = f.fileSize()==0 ? 0 : min(f.fileSize()/254+1, 9999);
              m_dirBuffer[m_dirBufferLen++] = 1;
              m_dirBuffer[m_dirBufferLen++] = 1;
              m_dirBuffer[m_dirBufferLen++] = size&255;
              m_dirBuffer[m_dirBufferLen++] = size/256;
              if( size<10 )    m_dirBuffer[m_dirBufferLen++] = ' ';
              if( size<100 )   m_dirBuffer[m_dirBufferLen++] = ' ';
              if( size<1000 )  m_dirBuffer[m_dirBufferLen++] = ' ';

              m_dirBuffer[m_dirBufferLen++] = '"';
              size_t n = f.getName(m_dirBuffer+m_dirBufferLen, 22);
#if SDFAT_FILE_TYPE == 3
              if( (f.attrib() & (FS_ATTRIB_SYSTEM | FS_ATTRIB_HIDDEN))!=0 ) n = 0;
#else
              if( f.isSystem() ) 
                n = 0;
              else if( n==0 ) 
                n = f.getSFN(m_dirBuffer+m_dirBufferLen, 22);
#endif
              if( n>0 )
                {
                  toPETSCII((byte *) m_dirBuffer+m_dirBufferLen);

                  const char *ftype = NULL;
                  if( n>4 && strcasecmp_P(m_dirBuffer+m_dirBufferLen+n-4, PSTR(".prg"))==0 )
                    { n -= 4; ftype = PSTR("PRG"); }
                  else if( n>4 && strcasecmp(m_dirBuffer+m_dirBufferLen+n-4, PSTR(".seq"))==0 )
                    { n -= 4; ftype = PSTR("SEQ"); }

                  m_dirBufferLen += n;
                  m_dirBuffer[m_dirBufferLen++] = '"';
                  m_dirBuffer[m_dirBufferLen] = 0;
                  
                  m_dirBufferLen += strlen(m_dirBuffer+m_dirBufferLen);
                  n = 17-n;
                  while(n-->0) m_dirBuffer[m_dirBufferLen++] = ' ';
                  strcpy_P(m_dirBuffer+m_dirBufferLen, ftype!=NULL ? ftype : (f.isDir() ? PSTR("DIR") : PSTR("PRG")));
                  m_dirBufferLen+=3;
                  while( m_dirBufferLen<31 ) m_dirBuffer[m_dirBufferLen++] = ' ';
                  m_dirBuffer[m_dirBufferLen++] = 0;
                  repeat = false;
                }

              f.close();
            }
          else
            {
              uint32_t free = min(65535, m_sd.bytesPerCluster() * m_sd.clusterCount() / 254);
              m_dirBuffer[0] = 1;
              m_dirBuffer[1] = 1;
              m_dirBuffer[2] = free&255;
              m_dirBuffer[3] = free/256;
              strcpy_P(m_dirBuffer+4, PSTR("BLOCKS FREE.             "));
              m_dirBuffer[29] = 0;
              m_dirBuffer[30] = 0;
              m_dirBuffer[31] = 0;
              m_dirBufferLen = 32;
              m_dir.close();
              repeat = false;
            }
        }
    } 
      
  if( m_dirBufferPtr<m_dirBufferLen )
    {
      *data = m_dirBuffer[m_dirBufferPtr++];
      return true;
    }
  else
    return false;
}


bool IECSD::isMatch(const char *name, const char *pattern, byte extmatch)
{
  signed char found = -1;

  for(byte i=0; found<0; i++)
    {
      if( pattern[i]=='*' )
        found = 1;
      else if( pattern[i]==0 && name[i]=='.' )
        {
          if( (extmatch & 1) && strcasecmp_P(name+i+1, PSTR("prg"))==0 )
            found = 1;
          else if( (extmatch & 2) && strcasecmp_P(name+i+1, PSTR("seq"))==0 )
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


const char *IECSD::findFile(const char *pattern, char ftype)
{
  bool found = false;
  static char name[22];

  if( m_dir.openCwd() )
    {

      m_file.close();
      while( !found && m_file.openNext(&m_dir, O_RDONLY) )
        {
          found = !m_file.isDir() && m_file.getName(name, 22) && isMatch(name, pattern, ftype=='P' ? 1 : 2);
          m_file.close();
        }
      
      m_dir.close();
    }

  return found ? name : NULL;
}


byte IECSD::openFile(byte channel, const char *constName)
{
  byte res = E_OK;
  char ftype = 'P';
  char mode  = 'R';
  char *name = m_dirBuffer;

  strcpy(name, constName);
  fromPETSCII((byte *) name);

  char *comma = strchr(name, ',');
  if( comma!=NULL )
    {
      char *c = comma;
      do { *c-- = 0; } while( c!=name && ((*c) & 0x7f)==' ');
      ftype  = toupper(*(comma+1));
      comma  = strchr(comma+1, ',');
      if( comma!=NULL )
        mode = toupper(*(comma+1));
    }
  else if( channel==0 )
    mode = 'R';
  else if( channel==1 )
    mode = 'W';
  
  if( (ftype!='P' && ftype!='S') || (mode!='R' && mode!='W') )
    res = E_INVNAME;

  if( res == E_OK )
    {
      if( mode=='R' )
        {
          if( name[0]==':' ) name++;

          if( !m_file.open(name, O_RDONLY)
#if SDFAT_FILE_TYPE == 1
              && !m_file.openExistingSFN(name) 
#endif
              )
            {
              const char *fn = findFile(name, ftype);
              if( fn ) m_file.open(fn, O_RDONLY);
            }
          
          res = m_file.isOpen() && (m_file.fileSize()>0) ? E_OK : E_NOTFOUND;
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
          if( overwrite && findFile(name, '\0')!=NULL )
            {
              m_dir.openCwd();
              m_dir.remove(name);
              m_dir.close();
            }
          
          strcat_P(name, ftype=='P' ? PSTR(".prg") : PSTR(".seq"));
          if( m_file.open(name, O_WRONLY | O_CREAT | (overwrite ? 0 : O_EXCL)) )
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


void IECSD::open(byte device, byte channel, const char *name)
{
  if( !checkCard() )
    m_errorCode = E_NOTREADY;
  else if( channel==0 && name[0]=='$' )
    m_errorCode = openDir();
  else if ( !m_file )
    m_errorCode = openFile(channel, name);
  else
    {
      // we can only have one file open at a time
      m_errorCode = E_TOOMANY;
    }

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus(device);
}


byte IECSD::read(byte device, byte channel, byte *buffer, byte bufferSize)
{
  if( m_file.isOpen() )
    return m_file.read(buffer, bufferSize);
  else
    return readDir(buffer) ? 1 : 0;
}


byte IECSD::write(byte device, byte channel, byte *buffer, byte bufferSize)
{
  if( m_file.isOpen() )
    return m_file.write(buffer, bufferSize);
  else
    return 0;
}


void IECSD::close(byte device, byte channel)
{
  if( m_dir )
    { 
      m_dir.close();
      m_dirBufferLen = 0;
    }
  else 
    m_file.close(); 
}


void IECSD::execute(byte device, const char *command, byte len)
{
  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus(device);

  if( strncmp(command, "S:", 2)==0 )
    {
      if( m_dir.openCwd() )
        {
          char pattern[17];
          m_errorCode = E_SCRATCHED;
          m_scratched = 0;

          strncpy(pattern, command+2, 16);
          pattern[16]=0;
          fromPETSCII((byte *) pattern);
          
          while( m_file.openNext(&m_dir, O_RDONLY) )
            {
              size_t n = m_file.getName(m_dirBuffer, 22);
              m_file.close();
              if( n>0 && isMatch(m_dirBuffer, pattern, 1+2) && m_dir.remove(m_dirBuffer) )
                m_scratched++;
            }
          
          m_dir.close();
        }
      else
        m_errorCode = E_NOTREADY;
    }
  else if( strncmp_P(command, PSTR("M-R\xfa\x02\x03"), 6)==0 )
    {
      // hack: DolphinDos' MultiDubTwo reads 02FA-02FC to determine
      // number of free blocks => pretend we have 664 (0298h) blocks available
      byte data[3] = {0x98, 0, 0x02};
      setStatus(device, (char *) data, 3);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-R"), 3)==0 && len>=6 && command[5]<=32 )
    {
      // memory read not supported => always return 0xFF
      byte n = command[5];
      char buf[32];
      memset(buf, 0xFF, n);
      setStatus(device, buf, n);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-W"), 3)==0 )
    {
      // memory write not supported => ignore
      m_errorCode = E_OK;
    }
  else if( (strncmp_P(command, PSTR("CD:"),3)==0 || strncmp_P(command, PSTR("MD:"),3)==0 || strncmp_P(command, PSTR("RD:"),3)==0) && command[3]!=0 )
    {
      strncpy(m_dirBuffer, command+3, 16);
      m_dirBuffer[16]=0;
      fromPETSCII((byte *) m_dirBuffer);

      if( command[0]=='C' )
        {
          // The SdFat library keeps track of the current directory but it does
          // not provide a way to go one directory up (i.e. "cd .."). As a workaround
          // we just always go back to the root directory after a "cd .."
          if( strcmp_P(m_dirBuffer, PSTR(".."))==0 )
            m_errorCode = m_sd.chdir("/") ? E_OK : E_NOTREADY;
          else
            m_errorCode = m_sd.chdir(m_dirBuffer) ? E_OK : E_NOTFOUND;
        }
      else if( command[0]=='M' )
        m_errorCode = m_sd.mkdir(m_dirBuffer, true) ? E_OK : E_EXISTS;
      else if( command[0]=='R' )
        m_errorCode = m_sd.rmdir(m_dirBuffer) ? E_OK : E_NOTFOUND;
    }
  else if( strcmp_P(command, PSTR("CD_"))==0 )
    m_errorCode = m_sd.chdir("/") ? E_OK : E_NOTREADY;
  else if( strcmp(command, "I")==0 || strcmp_P(command, PSTR("X+\x0dUJ"))==0 )
    m_errorCode = E_OK;
  else
    m_errorCode = E_INVCMD;
}


void IECSD::getStatus(byte device, char *buffer, byte bufferSize)
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
    case E_INVCMD:
    case E_INVNAME:              { message = PSTR("SYNTAX ERROR"); break; }
    case E_TOOMANY:              { message = PSTR("TOO MANY OPEN FILES"); break; }
    case E_SPLASH:               { message = PSTR("IECSD V0.1"); break; }
    default:                     { message = PSTR("UNKNOWN"); break; }
    }

  byte i = 0;
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
  unsigned long ledTestEnd = millis() + 250;
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);

  IECFileDevice::reset();

  m_cardOk = false;
  m_errorCode = E_SPLASH;
  if( !checkCard() ) m_errorCode = E_NOTREADY;

  m_file.close();
  m_dir.close();

  if( m_pinLED<0xFF ) 
    { 
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      digitalWrite(m_pinLED, LOW); 
    }
}
