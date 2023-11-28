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


IECSD::IECSD(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinChipSelect, byte pinLED) :
  IECDevice(pinATN, pinCLK, pinDATA, pinRESET)
{
  m_pinLED = pinLED;
  m_pinChipSelect = pinChipSelect;
}


void IECSD::begin(byte devnr)
{
  IECDevice::begin(devnr);
  pinMode(m_pinLED, OUTPUT);
  m_curCmd = CMD_NONE;
  m_opening = false;
  m_errorCode = E_SPLASH;

  m_dataBufferPtr = 0;
  m_dataBufferLen = 0;
  m_statusBufferPtr = 0;
  m_statusBufferLen = 0;

  if( !SD.begin(m_pinChipSelect) )
    m_errorCode = E_NOTREADY;
}


void IECSD::task()
{
  static unsigned long blinkto = 0;
  if( m_errorCode!=E_OK && m_errorCode!=E_SPLASH )
    {
      if( millis()>blinkto )
        {
          digitalWrite(m_pinLED, !digitalRead(m_pinLED));
          blinkto += 500;
        }
    }

  // handle SD card operations
  sdTask();

  // handle IEC serial bus communication
  IECDevice::task();
}


void IECSD::sdTask()
{
  switch( m_curCmd )
    {
    case CMD_COMMAND:
      {
        m_statusBufferPtr = m_statusBufferLen;

        if( m_dataBuffer[0]=='U' )
          {
            if( m_dataBuffer[1]=='9' || m_dataBuffer[1]=='I' ) reset();
          }
        else if( strncmp(m_dataBuffer, "S:", 2)==0 )
          {
            m_errorCode = E_SCRATCHED;
            m_scratched = 0;
            Serial.println(m_dataBuffer+2);
            if( SD.remove(m_dataBuffer+2) ) m_scratched++;
          }
        else if( strcmp(m_dataBuffer, "I")==0 )
          {
            m_errorCode = E_OK;
          }
        else
          m_errorCode = E_INVCMD;

        digitalWrite(m_pinLED, LOW);
        m_curCmd = CMD_NONE;
        break;
      }
      
    case CMD_READFILE:
      {
        if( !m_file )
          {
            m_dataBufferLen = 0;
            m_dataBufferPtr = 0;

            char *c = m_dataBuffer;
            if( m_dataBuffer[1]==':' ) c+=2;
            m_file = SD.open(c, FILE_READ);

            m_errorCode = m_file && m_file.size()>0 ? E_OK : E_NOTFOUND;
            m_statusBufferPtr = m_statusBufferLen;
            m_fileWrite = false;

            if( m_errorCode != E_OK )
              {
                m_curCmd = CMD_NONE;
                m_file.close();
              }
          }

        if( m_dataBufferPtr+2>=m_dataBufferLen && m_curCmd==CMD_READFILE )
          {
            memmove(m_dataBuffer, m_dataBuffer+m_dataBufferPtr, m_dataBufferLen-m_dataBufferPtr);
            m_dataBufferLen -= m_dataBufferPtr;
            m_dataBufferPtr = 0;

            int count = m_file.read(m_dataBuffer+m_dataBufferLen, IECSD_BUFSIZE-m_dataBufferLen);
            if( count<0 )
              {
                m_file.close();
                m_errorCode = E_READ;
                m_curCmd = CMD_NONE;
                m_dataBufferLen = 0;
              }
            else if( count==0 )
              {
                m_file.close();
                m_curCmd = CMD_NONE;
              }
            else
              {
                m_dataBufferLen += count;
              }
          }

        break;
      }

    case CMD_WRITEFILE:
      {
        if( !m_file )
          {
            bool overwrite = false;
            char *c = m_dataBuffer;
            if( c[0]=='@' && c[1]==':' )
              { c+=2; overwrite = true; }
            else if( c[0]=='@' && c[1]!=0 && c[2]==':' )
              { c+=3; overwrite = true; }
            else if( c[0]!=0 && c[1]==':' )
              { c+=2; }

            m_statusBufferPtr = m_statusBufferLen;
            m_errorCode = E_OK;
            if( SD.exists(c) )
              {
                if( !overwrite )
                  m_errorCode = E_EXISTS;
                else if( !SD.remove(c) )
                  m_errorCode = E_WRITE;
              }
            else if( !(m_file = SD.open(c, FILE_WRITE)) )
              m_errorCode = E_WRITE;

            if( m_errorCode == E_OK )
              {
                m_fileWrite = true;
                m_dataBufferLen = 0;
              }
            else
              {
                m_curCmd = CMD_NONE;
              }
          }

        if( m_dataBufferLen==IECSD_BUFSIZE && m_curCmd==CMD_WRITEFILE )
          {
            int count = m_file.write(m_dataBuffer, IECSD_BUFSIZE);
            
            if( count==0 )
              {
                m_file.close();
                m_curCmd = CMD_NONE;
                m_dataBufferLen = 0;
                m_errorCode = E_WRITE;
              }
            else
              {
                memmove(m_dataBuffer, m_dataBuffer+count, IECSD_BUFSIZE-count);
                m_dataBufferLen -= count;
              }
          }

        break;
      }

    case CMD_CLOSEFILE:
      {
        m_errorCode = E_OK; 
        if( m_fileWrite && m_dataBufferLen>0 )
          {
            if( m_file.write(m_dataBuffer, m_dataBufferLen)==0 )
              m_errorCode = E_WRITE;
          }

        m_file.close(); 
        
        m_curCmd = CMD_NONE;
        m_dataBufferLen = 0;
        break;
      }

    case CMD_READDIR:
      {
        if( m_dataBufferLen==0 )
          {
            m_statusBufferPtr = m_statusBufferLen;
            m_dir = SD.open("/");
            if( m_dir )
              {
                m_dataBuffer[0] = 0x00;
                m_dataBuffer[1] = 0x04;
                m_dataBufferLen = 2;
                m_dataBufferPtr = 0;

                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = 0;
                m_dataBuffer[m_dataBufferLen++] = 0;
                m_dataBuffer[m_dataBufferLen++] = 18;
                m_dataBuffer[m_dataBufferLen++] = '"';
                int n = m_dataBufferLen;
                strcpy(m_dataBuffer+m_dataBufferLen, m_dir.name());
                m_dataBufferLen += strlen(m_dataBuffer+m_dataBufferLen);
                if( m_dataBuffer[m_dataBufferLen-1]=='/' ) m_dataBuffer[--m_dataBufferLen]=0; 
                n = 17-strlen(m_dataBuffer+n);
                while( n-->0 ) m_dataBuffer[m_dataBufferLen++] = ' ';
                m_dataBuffer[m_dataBufferLen++] = '"';
                strcpy(m_dataBuffer+m_dataBufferLen, " 00 2A");
                m_dataBufferLen += strlen(m_dataBuffer+m_dataBufferLen);
                m_dataBuffer[m_dataBufferLen++] = 0;
                m_errorCode = E_OK;
              }
            else
              {
                m_errorCode = E_NOTREADY;
                m_curCmd = CMD_NONE;
              }
          }

        if( m_curCmd==CMD_READDIR && m_dataBufferPtr+2>=m_dataBufferLen )
          {
            memmove(m_dataBuffer, m_dataBuffer+m_dataBufferPtr, m_dataBufferLen-m_dataBufferPtr);
            m_dataBufferLen -= m_dataBufferPtr;
            m_dataBufferPtr = 0;

            File f = m_dir.openNextFile();
            if( !f )
              {
                Sd2Card card;
                SdVolume volume; 
                uint32_t free = 0;
                if( card.init(SPI_HALF_SPEED, m_pinChipSelect) && volume.init(card) ) 
                  free = min(65535, volume.blocksPerCluster() * volume.clusterCount() * 2);
                SD.begin(m_pinChipSelect);
                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = free&255;
                m_dataBuffer[m_dataBufferLen++] = free/256;
                strcpy(m_dataBuffer+m_dataBufferLen, "BLOCKS FREE.");
                m_dataBufferLen += strlen(m_dataBuffer+m_dataBufferLen)+1;
                m_dataBuffer[m_dataBufferLen++] = 0;
                m_dataBuffer[m_dataBufferLen++] = 0;
                m_dir.close();
                m_curCmd = CMD_NONE;
              }
            else
              {
                uint16_t size = f.size()==0 ? 0 : min(f.size()/254+1, 65535);
                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = 1;
                m_dataBuffer[m_dataBufferLen++] = size&255;
                m_dataBuffer[m_dataBufferLen++] = size/256;
                if( size<10 )    m_dataBuffer[m_dataBufferLen++] = ' ';
                if( size<100 )   m_dataBuffer[m_dataBufferLen++] = ' ';
                if( size<1000 )  m_dataBuffer[m_dataBufferLen++] = ' ';
                if( size<10000 ) m_dataBuffer[m_dataBufferLen++] = ' ';

                m_dataBuffer[m_dataBufferLen++] = '"';
                strcpy(m_dataBuffer+m_dataBufferLen, f.name());
                m_dataBufferLen += strlen(m_dataBuffer+m_dataBufferLen);
                m_dataBuffer[m_dataBufferLen++] = '"';
                m_dataBuffer[m_dataBufferLen] = 0;
                
                m_dataBufferLen += strlen(m_dataBuffer+m_dataBufferLen);
                int n = 17-strlen(f.name());
                while(n-->0) m_dataBuffer[m_dataBufferLen++] = ' ';
                strcpy(m_dataBuffer+m_dataBufferLen, f.isDirectory() ? "DIR" : "PRG");
                m_dataBufferLen+=4;
                f.close();
              }
          }

        break;
      }
    }
}


int8_t IECSD::canWrite(byte channel)
{
  // if an error occurred when writing a file then m_curCmd will be
  // set to CMD_NONE, causing us to return 0 to signal the error
  return ((m_opening || channel==15 || m_curCmd==CMD_WRITEFILE) && m_dataBufferLen<IECSD_BUFSIZE) ? 1 : 0;
}


void IECSD::write(byte channel, byte data)
{
  // this function must return withitn 1000 microseconds
  // => do not add Serial.print or other function call that may take longer!

  m_dataBuffer[m_dataBufferLen++] = ((m_opening || channel==15) && data==0xFF) ? 0x7E : data;
}


int8_t IECSD::canRead(byte channel)
{
  if( channel==15 )
    {
      if( m_statusBufferPtr==m_statusBufferLen )
        setStatus();

      return min(2, m_statusBufferLen-m_statusBufferPtr);
    }
  else
    {
      // if an error occurred when reading a file then
      // the data buffer will be empty, causing us to return 0
      // which will signal the error condition to the receiver
      // ("file not found error")
      return min(2, m_dataBufferLen-m_dataBufferPtr);
    }
}


byte IECSD::read(byte channel)
{
  if( channel==15 )
    return m_statusBuffer[m_statusBufferPtr++];
  else
    return m_dataBuffer[m_dataBufferPtr++];
}


void IECSD::open(byte channel)
{
  m_opening = true;
  m_dataBufferLen = 0;
  m_dataBufferPtr = 0;
}


void IECSD::close(byte channel)
{
  if( m_file )
    m_curCmd = CMD_CLOSEFILE;
  else if( m_dir )
    {
      m_dir.close();
      m_curCmd = CMD_NONE;
      m_dataBufferLen = 0;
      m_dataBufferPtr = 0;
    }
  else
    {
      m_dataBufferLen = 0;
      m_dataBufferPtr = 0;
    }

  digitalWrite(m_pinLED, LOW);
}


void IECSD::setStatus()
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

  m_statusBufferLen = 0;
  m_statusBuffer[m_statusBufferLen++] = '0' + (m_errorCode / 10);
  m_statusBuffer[m_statusBufferLen++] = '0' + (m_errorCode % 10);
  m_statusBuffer[m_statusBufferLen++] = ',';
  strcpy_P(m_statusBuffer+m_statusBufferLen, message);
  m_statusBufferLen += strlen_P(message);

  if( m_errorCode!=E_SCRATCHED ) m_scratched = 0;
  m_statusBuffer[m_statusBufferLen++] = ',';
  m_statusBuffer[m_statusBufferLen++] = '0' + (m_scratched / 10);
  m_statusBuffer[m_statusBufferLen++] = '0' + (m_scratched % 10);
  strcpy_P(m_statusBuffer+m_statusBufferLen, PSTR(",00\r"));
  m_statusBufferLen += 4;
  m_statusBufferPtr = 0;
  m_statusBufferLen = strlen(m_statusBuffer);
  m_errorCode = E_OK;
  digitalWrite(m_pinLED, LOW);
}


void IECSD::listen(byte channel)
{
  if( channel==15 )
    m_dataBufferLen = 0;
}


void IECSD::unlisten(byte channel)
{
  if( channel==15 )
    {
      // execute DOS command
      if( m_dataBufferLen>0 )
        {
          m_dataBuffer[m_dataBufferLen]=0;
          m_curCmd = CMD_COMMAND;
        }

      m_opening = false;
    }
  else if( m_opening )
    {
      if( channel==0 && m_dataBufferLen<=2 && m_dataBuffer[0]=='$' )
        {
          // read directory
          m_curCmd = CMD_READDIR;
          m_dataBufferLen = 0;
          m_dataBufferPtr = 0;
          digitalWrite(m_pinLED, HIGH);
        }
      else if ( !m_file )
        {
          m_dataBuffer[m_dataBufferLen] = 0;

          if( channel==0 )
            m_curCmd = CMD_READFILE;
          else if( channel==1 )
            m_curCmd = CMD_WRITEFILE;
          else
            {
              char ftype = 'P';
              char mode  = 'R';

              m_dataBuffer[m_dataBufferLen] = 0;
              char *comma = strchr((char *) m_dataBuffer, ',');
              if( comma!=NULL )
                {
                  *comma = 0;
                  ftype  = *(comma+1);
                  comma  = strchr(comma+1, ',');
                  if( comma!=NULL )
                    mode = *(comma+1);
                }

              if( ftype=='P' || ftype=='S' )
                {
                  if( mode=='R' )
                    m_curCmd = CMD_READFILE;
                  else if( mode=='W' )
                    m_curCmd = CMD_WRITEFILE;
                  else
                    m_errorCode = E_INVNAME;
                }
              else
                m_errorCode = E_INVNAME;
            }

          if( m_errorCode==E_OK ) digitalWrite(m_pinLED, HIGH);
        }
      else
        {
          // we can only have one file open at a time
          m_curCmd = CMD_NONE;
          m_errorCode = E_TOOMANY;
        }

      m_opening = false;
    }
}


void IECSD::reset()
{
  m_curCmd = CMD_NONE;
  m_opening = false;
  m_errorCode = E_SPLASH;
  m_file.close();
  m_dir.close();
  digitalWrite(m_pinLED, LOW);
}
