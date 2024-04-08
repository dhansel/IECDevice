// -----------------------------------------------------------------------------
// Copyright (C) 2023 David Hansel
//
// This implementation is based on the code used in the VICE emulator.
// The corresponding code in VICE (src/serial/serial-iec-device.c) was 
// contributed to VICE by me in 2003.
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

#define DEBUG 0

#if DEBUG>0

void print_hex(byte data)
{
  static const PROGMEM char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  Serial.write(pgm_read_byte_near(hex+(data/16)));
  Serial.write(pgm_read_byte_near(hex+(data&15)));
}


static byte dbgbuf[16], dbgnum = 0;

void dbg_print_data()
{
  if( dbgnum>0 )
    {
      for(byte i=0; i<dbgnum; i++)
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

void dbg_data(byte data)
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


IECFileDevice::IECFileDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL) : 
  IECDevice(pinATN, pinCLK, pinDATA, pinRESET, pinCTRL)
{
  m_cmd = IFD_NONE;

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
  m_canServeATN = (m_pinCTRL!=0xFF) || (m_atnInterrupt != NOT_AN_INTERRUPT);
}


void IECFileDevice::begin(byte devnr)
{
#if DEBUG>0
  /*if( !Serial )*/ Serial.begin(115200);
  Serial.print(F("START:IECFileDevice, devnr=")); Serial.println(devnr);
#endif
  
  IECDevice::begin(devnr);
#if JIFFY_BUFFER_SIZE>0
  IECDevice::enableJiffyDosSupport(m_jiffyBuffer, JIFFY_BUFFER_SIZE);
#endif
  m_statusBufferPtr = 0;
  m_statusBufferLen = 0;
  memset(m_dataBufferLen, 0, 15);
  m_cmd = IFD_NONE;
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
          m_statusBuffer[0] = 0;
          getStatus(m_statusBuffer, 31);
          m_statusBuffer[31] = 0;
#if DEBUG>0
          Serial.print(F("STATUS: ")); Serial.println(m_statusBuffer);
          for(byte i=0; m_statusBuffer[i]; i++) dbg_data(m_statusBuffer[i]);
          dbg_print_data();
#endif

          m_statusBufferLen = strlen(m_statusBuffer);
          m_statusBufferPtr = 0;
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


byte IECFileDevice::peek() 
{
  byte data;

  if( m_channel==15 )
    data = m_statusBuffer[m_statusBufferPtr];
  else 
    data = m_dataBuffer[m_channel][0];

#if DEBUG>1
  Serial.write('P'); print_hex(data);
#endif

  return data;
}


byte IECFileDevice::read() 
{ 
  byte data;

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


byte IECFileDevice::read(byte *buffer, byte bufferSize)
{
  byte res = 0;

  // get data from our own 2-byte buffer (if any)
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
      byte n = read(m_channel, buffer+res, bufferSize-res);
      if( n==0 ) break;
#if DEBUG>0
      for(byte i=0; i<n; i++) dbg_data(buffer[res+i]);
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


void IECFileDevice::write(byte data) 
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
  else if( m_nameBufferLen<32 )
    m_nameBuffer[m_nameBufferLen++] = data;

#if DEBUG>1
  Serial.write('W'); print_hex(data);
#endif
}


void IECFileDevice::talk(byte secondary)   
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
}


void IECFileDevice::listen(byte secondary) 
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
    }
  else if( m_opening )
    {
      m_opening = false;
      m_nameBuffer[m_nameBufferLen] = 0;
      m_cmd = IFD_OPEN;
    }
}


void IECFileDevice::fileTask()
{
  switch( m_cmd )
    {
    case IFD_OPEN:
      {
#if DEBUG>0
        for(byte i=0; m_nameBuffer[i]; i++) dbg_data(m_nameBuffer[i]);
        dbg_print_data();
        Serial.print(F("OPEN #")); Serial.print(m_channel); Serial.print(F(": ")); Serial.println(m_nameBuffer);
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
        if( write(m_channel, m_dataBuffer[m_channel][0]) ) 
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
        Serial.print(F("CLOSE #")); Serial.println(m_channel);
#endif
        close(m_channel); 
        m_dataBufferLen[m_channel] = 0;
        break;
      }

    case IFD_EXEC:  
      {
#if DEBUG>0
        Serial.println((int) m_nameBufferLen);
        for(byte i=0; i<m_nameBufferLen; i++) dbg_data(m_nameBuffer[i]);
        dbg_print_data();
        Serial.println(F("EXECUTE: ")); Serial.println(m_nameBuffer);
#endif
        execute(m_nameBuffer, m_nameBufferLen); 
        break;
      }
    }
  
  m_cmd = IFD_NONE;
}


void IECFileDevice::setStatus(char *data, byte dataLen)
{
#if DEBUG>0
  Serial.print(F("SETSTATUS ")); Serial.println(dataLen);
#endif

  m_statusBufferPtr = 0;
  m_statusBufferLen = min((byte) 32, dataLen);
  memcpy(m_statusBuffer, data, m_statusBufferLen);
}


void IECFileDevice::reset()
{
  IECDevice::reset();

#if DEBUG>0
  Serial.println(F("RESET"));
#endif

  m_statusBufferPtr = 0;
  m_statusBufferLen = 0;
  memset(m_dataBufferLen, 0, 15);
  m_cmd = IFD_NONE;
}


void IECFileDevice::task()
{
  IECDevice::task();

  // see comment in IECFileDevice constructor
  if( m_canServeATN ) fileTask();
}
