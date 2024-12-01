// -----------------------------------------------------------------------------
// Copyright (C) 2024 David Hansel
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

#include "IECFileDevice.h"
#include "IECBusHandler.h"

#if defined(ARDUINO)
#include <Arduino.h>
#elif defined(ESP_PLATFORM)
#include "IECespidf.h"
#endif

#define DEBUG 0

#if DEBUG>0

void print_hex(uint8_t data)
{
  static const PROGMEM char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  Serial.write(pgm_read_byte_near(hex+(data/16)));
  Serial.write(pgm_read_byte_near(hex+(data&15)));
}


static uint8_t dbgbuf[16], dbgnum = 0;

void dbg_print_data()
{
  if( dbgnum>0 )
    {
      for(uint8_t i=0; i<dbgnum; i++)
        {
          if( i==8 ) Serial.write(' ');
          print_hex(dbgbuf[i]);
          Serial.write(' ');
        }

      for(int i=0; i<(16-dbgnum)*3; i++) Serial.write(' ');
      if( dbgnum<8 ) Serial.write(' ');
      for(int i=0; i<dbgnum; i++)
        {
          if( (i&7)==0 ) Serial.write(' ');
          Serial.write(isprint(dbgbuf[i]) ? dbgbuf[i] : '.');
        }
      Serial.write('\n');
      dbgnum = 0;
    }
}

void dbg_data(uint8_t data)
{
  dbgbuf[dbgnum++] = data;
  if( dbgnum==16 ) dbg_print_data();
}

#endif


#define IFD_NONE  0
#define IFD_OPEN  1
#define IFD_READ  2
#define IFD_WRITE 3
#define IFD_CLOSE 4
#define IFD_EXEC  5


IECFileDevice::IECFileDevice(uint8_t devnr) : 
  IECDevice(devnr)
{
  m_cmd = IFD_NONE;
  m_opening = false;
}


void IECFileDevice::begin()
{
#if DEBUG>0
  /*if( !Serial )*/ Serial.begin(115200);
  for(int i=0; !Serial && i<5; i++) delay(1000);
  Serial.print(F("START:IECFileDevice, devnr=")); Serial.println(m_devnr);
#endif
  
  bool ok;
#ifdef SUPPORT_JIFFY
  ok = IECDevice::enableJiffyDosSupport(true);
#if DEBUG>0
  Serial.print(F("JiffyDos support ")); Serial.println(ok ? F("enabled") : F("disabled"));
#endif
#endif
#ifdef SUPPORT_DOLPHIN
  ok = IECDevice::enableDolphinDosSupport(true);
#if DEBUG>0
  Serial.print(F("DolphinDos support ")); Serial.println(ok ? F("enabled") : F("disabled"));
#endif
#endif
#ifdef SUPPORT_EPYX
  ok = IECDevice::enableEpyxFastLoadSupport(true);
  m_epyxCtr = 0;
#if DEBUG>0
  Serial.print(F("Epyx FastLoad support ")); Serial.println(ok ? F("enabled") : F("disabled"));
#endif
#endif

  m_statusBufferPtr = 0;
  m_statusBufferLen = 0;
  memset(m_dataBufferLen, 0, 15);
  m_cmd = IFD_NONE;
  m_opening = false;

  // calling fileTask() may result in significant time spent accessing the
  // disk during which we can not respond to ATN requests within the required
  // 1000us (interrupts are disabled during disk access). We have two options:
  // 1) call fileTask() from within the canWrite() and canRead() functions
  //    which are allowed to block indefinitely. Doing so has two downsides:
  //    - receiving a disk command via OPEN 1,x,15,"CMD" will NOT execute
  //      it right away because there is no call to canRead/canWrite after
  //      the "unlisten" call that finishes the transmission. The command will
  //      execute once the next operation (even just a status query) starts.
  //    - if the bus master pulls ATN low in the middle of a transmission
  //      (does not usually happen) we may not respond fast enough which may
  //      result in a "Device not present" error.
  // 2) add some minimal hardware that immediately pulls DATA low when ATN
  //    goes low (this is what the C1541 disk drive does). This will make
  //    the bus master wait until we release DATA when we are actually ready
  //    to communicate. In that case we can process fileTask() here which
  //    mitigates both issues with the first approach. The hardware needs
  //    one additional output pin (pinCTRL) used to enable/disable the
  //    override of the DATA line.
  //
  // if we have the extra hardware then m_pinCTRL!=0xFF 
  m_canServeATN = m_handler->canServeATN();

  IECDevice::begin();
}


uint8_t IECFileDevice::getStatusData(char *buffer, uint8_t bufferSize) 
{ 
  // call the getStatus() function that returns a null-terminated string
  m_statusBuffer[0] = 0;
  getStatus(m_statusBuffer, bufferSize);
  m_statusBuffer[bufferSize-1] = 0;
  return strlen(m_statusBuffer);
}


int8_t IECFileDevice::canRead() 
{ 
#if DEBUG>2
  Serial.write('c');Serial.write('R');
#endif

  // see comment in IECFileDevice constructor
  if( !m_canServeATN ) fileTask();

  if( m_channel==15 )
    {
      if( m_statusBufferPtr==m_statusBufferLen )
        {
          m_statusBufferPtr = 0;
          m_statusBufferLen = getStatusData(m_statusBuffer, IECFILEDEVICE_STATUS_BUFFER_SIZE);
#if DEBUG>0
          Serial.print(F("STATUS")); 
#if MAX_DEVICES>1
          Serial.write('#'); Serial.print(m_devnr);
#endif
          Serial.write(':'); Serial.write(' ');
          Serial.println(m_statusBuffer);
          for(uint8_t i=0; i<m_statusBufferLen; i++) dbg_data(m_statusBuffer[i]);
          dbg_print_data();
#endif
        }
      
      return m_statusBufferLen-m_statusBufferPtr;
    }
  else if( m_dataBufferLen[m_channel]<0 )
    {
      // first canRead() call after open()
      if( !read(m_channel, &(m_dataBuffer[m_channel][0]), 1) )
        {
          m_dataBufferLen[m_channel] = 0;
        }
      else if( !read(m_channel, &(m_dataBuffer[m_channel][1]), 1) )
        {
          m_dataBufferLen[m_channel] = 1;
#if DEBUG==1
          dbg_data(m_dataBuffer[m_channel][0]);
#endif
        }
      else
        {
          m_dataBufferLen[m_channel] = 2;
#if DEBUG==1
          dbg_data(m_dataBuffer[m_channel][0]);
          dbg_data(m_dataBuffer[m_channel][1]);
#endif
        }

      return m_dataBufferLen[m_channel];
    }
  else
    {
      return m_dataBufferLen[m_channel];
    }
}


uint8_t IECFileDevice::peek() 
{
  uint8_t data;

  if( m_channel==15 )
    data = m_statusBuffer[m_statusBufferPtr];
  else 
    data = m_dataBuffer[m_channel][0];

#if DEBUG>1
  Serial.write('P'); print_hex(data);
#endif

  return data;
}


uint8_t IECFileDevice::read() 
{ 
  uint8_t data;

  if( m_channel==15 )
    data = m_statusBuffer[m_statusBufferPtr++];
  else 
    {
      data = m_dataBuffer[m_channel][0];
      if( m_dataBufferLen[m_channel]==2 )
        {
          m_dataBuffer[m_channel][0] = m_dataBuffer[m_channel][1];
          m_dataBufferLen[m_channel] = 1;
          m_cmd = IFD_READ;
        }
      else
        m_dataBufferLen[m_channel] = 0;
    }

#if DEBUG>1
  Serial.write('R'); print_hex(data);
#endif

  return data;
}


uint8_t IECFileDevice::read(uint8_t *buffer, uint8_t bufferSize)
{
  uint8_t res = 0;

  // get data from our own 2-uint8_t buffer (if any)
  // properly deal with the case where bufferSize==1
  while( m_dataBufferLen[m_channel]>0 && res<bufferSize )
    {
      buffer[res++] = m_dataBuffer[m_channel][0];
      m_dataBuffer[m_channel][0] = m_dataBuffer[m_channel][1];
      m_dataBufferLen[m_channel]--;
    }

  // get data from higher class
  while( res<bufferSize )
    {
      uint8_t n = read(m_channel, buffer+res, bufferSize-res);
      if( n==0 ) break;
#if DEBUG>0
      for(uint8_t i=0; i<n; i++) dbg_data(buffer[res+i]);
#endif
      res += n;
    }
  
  return res;
}


int8_t IECFileDevice::canWrite() 
{
#if DEBUG>2
  Serial.write('c');Serial.write('W');
#endif

  // see comment in IECFileDevice constructor
  if( !m_canServeATN ) fileTask();

  return (m_opening || m_channel==15 || m_dataBufferLen[m_channel]<1) ? 1 : 0;
}


void IECFileDevice::write(uint8_t data, bool eoi) 
{
  // this function must return withitn 1 millisecond
  // => do not add Serial.print or function call that may take longer!
  // (at 115200 baud we can send 10 characters in less than 1 ms)

  if( m_channel<15 && !m_opening )
    {
      m_dataBuffer[m_channel][0] = data;
      m_dataBufferLen[m_channel] = 1;
      m_cmd = IFD_WRITE;
    }
  else if( m_nameBufferLen<IECFILEDEVICE_NAME_BUFFER_SIZE-1 )
    m_nameBuffer[m_nameBufferLen++] = data;

#if DEBUG>1
  Serial.write('W'); print_hex(data);
#endif
}


uint8_t IECFileDevice::write(uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  uint8_t nn;
  int8_t n = m_dataBufferLen[m_channel];
  if( n>0 )
    {
      nn = write(m_channel, m_dataBuffer[m_channel], n);
#if DEBUG>0
      for(uint8_t i=0; i<nn; i++) dbg_data(m_dataBuffer[m_channel][i]);
#endif
      n -= nn;
      m_dataBufferLen[m_channel] = n;
      if( n>0 ) return 0;
    }

  nn = write(m_channel, buffer, bufferSize);
#if DEBUG>0
  for(uint8_t i=0; i<nn; i++) dbg_data(buffer[i]);
#endif
  return nn;
}


void IECFileDevice::talk(uint8_t secondary)   
{
#if DEBUG>1
  Serial.write('T'); print_hex(secondary);
#endif

  m_channel = secondary & 0x0F;
}


void IECFileDevice::untalk() 
{
#if DEBUG>1
  Serial.write('t');
#endif

  // need to set channel to anything other than 15 so we don't go
  // into the m_channel==15 branch in unlisten() later
  m_channel = 0;
}


void IECFileDevice::listen(uint8_t secondary) 
{
#if DEBUG>1
  Serial.write('L'); print_hex(secondary);
#endif

  m_channel = secondary & 0x0F;
  
  if( m_channel==15 )
    m_nameBufferLen = 0;
  else if( (secondary & 0xF0) == 0xF0 )
    {
      m_opening = true;
      m_nameBufferLen = 0;
    }
  else if( (secondary & 0xF0) == 0xE0 )
    {
      m_cmd = IFD_CLOSE;
    }
}


void IECFileDevice::unlisten() 
{
#if DEBUG>1
  Serial.write('l');
#endif

  if( m_channel==15 )
    {
      if( m_nameBufferLen>0 )
        {
          if( m_nameBuffer[m_nameBufferLen-1]==13 ) m_nameBufferLen--;
          m_nameBuffer[m_nameBufferLen]=0;
          m_cmd = IFD_EXEC;
        }

      // need to set channel to anything other than 15 so we don't get here
      // again if we intercept an UNLISTEN for a different device
      m_channel = 0;
    }
  else if( m_opening )
    {
      m_opening = false;
      m_nameBuffer[m_nameBufferLen] = 0;
      m_cmd = IFD_OPEN;
    }
}


#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
bool IECFileDevice::epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
#if DEBUG>0
  dbg_print_data();
  Serial.print("Read track "); Serial.print(track); Serial.print(" sector "); Serial.println(sector);
  for(int i=0; i<256; i++) dbg_data(buffer[i]);
  dbg_print_data();
  Serial.flush();
  return true;
#else
  return false;
#endif
}


bool IECFileDevice::epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
#if DEBUG>0
  dbg_print_data();
  Serial.print("Write track "); Serial.print(track); Serial.print(" sector "); Serial.println(sector); Serial.flush();
  for(int i=0; i<256; i++) dbg_data(buffer[i]);
  dbg_print_data();
  return true;
#else
  return false;
#endif
}
#endif


void IECFileDevice::fileTask()
{
  switch( m_cmd )
    {
    case IFD_OPEN:
      {
#if DEBUG>0
        for(uint8_t i=0; m_nameBuffer[i]; i++) dbg_data(m_nameBuffer[i]);
        dbg_print_data();
        Serial.print(F("OPEN #")); 
#if MAX_DEVICES>1
        Serial.print(m_devnr); Serial.write('#');
#endif
        Serial.print(m_channel); Serial.print(F(": ")); Serial.println(m_nameBuffer);
#endif
        open(m_channel, m_nameBuffer); 
        m_dataBufferLen[m_channel] = -1;
        break;
      }
      
    case IFD_READ:
      {
        if( read(m_channel, &(m_dataBuffer[m_channel][m_dataBufferLen[m_channel]]), 1) )
          {
#if DEBUG==1
            dbg_data(m_dataBuffer[m_channel][m_dataBufferLen[m_channel]]);
#endif
            m_dataBufferLen[m_channel]++;
          }
        break;
      }

    case IFD_WRITE:
      {
        if( write(m_channel, m_dataBuffer[m_channel], 1)==1 )
          {
#if DEBUG==1
            dbg_data(m_dataBuffer[m_channel][0]);
#endif
            m_dataBufferLen[m_channel] = 0;
          }
        break;
      }
      
    case IFD_CLOSE: 
      {
#if DEBUG>0
        dbg_print_data();
        Serial.print(F("CLOSE #")); 
#if MAX_DEVICES>1
        Serial.print(m_devnr); Serial.write('#');
#endif
        Serial.println(m_channel);
#endif
        close(m_channel); 
        m_dataBufferLen[m_channel] = 0;
        break;
      }

    case IFD_EXEC:  
      {
        bool handled = false;
#if DEBUG>0
#if defined(SUPPORT_DOLPHIN)
        // Printing debug output here may delay our response to DolphinDos
        // 'XQ' and 'XZ' commands (burst mode request) too long and cause
        // the C64 to time out, causing the transmission to hang
        if( m_nameBuffer[0]!='X' || (m_nameBuffer[1]!='Q' && m_nameBuffer[1]!='Z') )
#endif
          {
            for(uint8_t i=0; i<m_nameBufferLen; i++) dbg_data(m_nameBuffer[i]);
            dbg_print_data();
            Serial.print(F("EXECUTE: ")); Serial.println(m_nameBuffer);
          }
#endif
#ifdef SUPPORT_EPYX
        if     ( m_epyxCtr== 0 && checkMWcmd(0x0180, 0x20, 0x2E) )
          { m_epyxCtr = 11; handled = true; }
        else if( m_epyxCtr==11 && checkMWcmd(0x01A0, 0x20, 0xA5) )
          { m_epyxCtr = 12; handled = true; }
        else if( m_epyxCtr==12 && strncmp_P(m_nameBuffer, PSTR("M-E\xa2\x01"), 5)==0 )
          { m_epyxCtr = 99; handled = true; } // EPYX V1
        else if( m_epyxCtr== 0 && checkMWcmd(0x0180, 0x19, 0x53) )
          { m_epyxCtr = 21; handled = true; }
        else if( m_epyxCtr==21 && checkMWcmd(0x0199, 0x19, 0xA6) )
          { m_epyxCtr = 22; handled = true; }
        else if( m_epyxCtr==22 && checkMWcmd(0x01B2, 0x19, 0x8F) )
          { m_epyxCtr = 23; handled = true; }
        else if( m_epyxCtr==23 && strncmp_P(m_nameBuffer, PSTR("M-E\xa9\x01"), 5)==0 )
          { m_epyxCtr = 99; handled = true; } // EPYX V2 or V3
        else
          m_epyxCtr = 0;

        if( m_epyxCtr==99 )
          {
#if DEBUG>0
            Serial.println(F("EPYX FASTLOAD DETECTED"));
#endif
            epyxLoadRequest();
            m_epyxCtr = 0;
          }
#endif
#ifdef SUPPORT_DOLPHIN
        if( strcmp_P(m_nameBuffer, PSTR("XQ"))==0 )
          { dolphinBurstTransmitRequest(); m_channel = 0; handled = true; }
        else if( strcmp_P(m_nameBuffer, PSTR("XZ"))==0 )
          { dolphinBurstReceiveRequest(); m_channel = 1; handled = true; }
        else if( strcmp_P(m_nameBuffer, PSTR("XF+"))==0 )
          { enableDolphinBurstMode(true); setStatus(NULL, 0); handled = true; }
        else if( strcmp_P(m_nameBuffer, PSTR("XF-"))==0 )
          { enableDolphinBurstMode(false); setStatus(NULL, 0); handled = true; }
#endif
        if( !handled ) execute(m_nameBuffer, m_nameBufferLen);
        break;
      }
    }
  
  m_cmd = IFD_NONE;
}


bool IECFileDevice::checkMWcmd(uint16_t addr, uint8_t len, uint8_t checksum) const
{
  uint8_t *buf = (uint8_t *) m_nameBuffer;

  // check buffer length and M-W command
  if( m_nameBufferLen<len+6 || strncmp_P(m_nameBuffer, PSTR("M-W"), 3)!=0 )
    return false;

  // check data length and destination address
  if( buf[3]!=(addr&0xFF) || buf[4]!=((addr>>8)&0xFF) || buf[5]!=len )
    return false;

  // check checksum
  uint8_t c = 0;
  for(uint8_t i=0; i<len; i++) c += buf[6+i];
  return c==checksum;
}


void IECFileDevice::setStatus(const char *data, uint8_t dataLen)
{
#if DEBUG>0
  Serial.print(F("SETSTATUS ")); 
#if MAX_DEVICES>1
  Serial.write('#'); Serial.print(m_devnr); Serial.write(' ');
#endif
  Serial.println(dataLen);
#endif

  m_statusBufferPtr = 0;
  m_statusBufferLen = min((uint8_t) IECFILEDEVICE_STATUS_BUFFER_SIZE, dataLen);
  memcpy(m_statusBuffer, data, m_statusBufferLen);
}


void IECFileDevice::clearStatus() 
{ 
  setStatus(NULL, 0); 
}


void IECFileDevice::reset()
{
#if DEBUG>0
#if MAX_DEVICES>1
  Serial.write('#'); Serial.print(m_devnr); Serial.write(' ');
#endif
  Serial.println(F("RESET"));
#endif

  m_statusBufferPtr = 0;
  m_statusBufferLen = 0;
  memset(m_dataBufferLen, 0, 15);
  m_cmd = IFD_NONE;
  m_opening = false;

#ifdef SUPPORT_EPYX
  m_epyxCtr = 0;
#endif

  IECDevice::reset();
}


void IECFileDevice::task()
{
  // see comment in IECFileDevice constructor
  if( m_canServeATN ) fileTask();
}
