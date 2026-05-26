// -----------------------------------------------------------------------------
// Copyright (C) 2026 David Hansel
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
// You should have receikved a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include "IECStringDevice.h"

#if defined(ARDUINO)
#include <Arduino.h>
#elif defined(ESP_PLATFORM)
#include "IECespidf.h"
#endif


IECStringDevice::IECStringDevice(uint8_t devnum, bool convertPETSCII) : IECDevice(devnum)
{
  m_commandLen = 0;
  m_responsePtr = 0;
  m_response[0] = 0;
  m_commandReceived = 0;
  m_convertPETSCII = convertPETSCII;
  m_defaultResponse = NULL;

  // make sure response and command are always 0-terminated
  m_command[IECSTRINGDEVICE_MAXLEN_COMMAND]=0;
  m_response[IECSTRINGDEVICE_MAXLEN_RESPONSE]=0;
}


int8_t IECStringDevice::canWrite()
{
  // if a command has been received (but not processed) then return -1
  // (i.e. cannot receive more data now), otherwise return 1 (ready to receive more)
  return m_commandReceived ? -1 : 1;
}


void IECStringDevice::write(uint8_t data, bool eoi)
{ 
  // make sure to not overrun the command buffer length (drop extra characters)
  if( m_commandLen<IECSTRINGDEVICE_MAXLEN_COMMAND )
    {
      if( m_convertPETSCII ) data = fromPETSCII(data);
      m_command[m_commandLen++] = (char) data;
    }

  // EOI means that this is the final byte of the transmission
  if( eoi )
    { 
      m_command[m_commandLen] = 0;
      m_commandReceived = true;
    }
}


int8_t IECStringDevice::canRead()
{
  // if we don't have a response to send then send the default response (if defined)
  if( m_response[m_responsePtr]==0 && m_defaultResponse!=NULL )
    {
      strncpy(m_response, m_defaultResponse, IECSTRINGDEVICE_MAXLEN_RESPONSE);
      m_responsePtr = 0;
    }

  // Return 0 if we have nothing to send. This will indicate a "nothing to send"
  // (error) condition on the bus. If we returned -1 instead then canRead()
  // would be called repeatedly, blocking the bus, until we have something to send.
  // That would prevent us from receiving incoming data on the bus.
  if( m_response[m_responsePtr]==0 )
    return 0; // no data to send
  else if( m_response[m_responsePtr+1]==0 )
    return 1; // only one byte left to send
  else
    return 2; // more than one byte left
}


uint8_t IECStringDevice::read()
{ 
  // read() will only be called if canRead() returned >0.
  uint8_t data = (uint8_t) m_response[m_responsePtr++];
  if( m_convertPETSCII ) data = toPETSCII(data);
  return data;
}


const char *IECStringDevice::getCommand()
{
  if( m_commandReceived )
    {
      m_commandReceived = false;
      m_commandLen = 0;
      return m_command;
    }
  else
    return NULL;
}


void IECStringDevice::setDefaultResponse(const char *s)
{
  m_defaultResponse = s;
}


void IECStringDevice::setResponse(const char *format, ...)
{
  va_list args;
  va_start (args, format);
  vsnprintf(m_response, IECSTRINGDEVICE_MAXLEN_RESPONSE, format, args);
  va_end (args);

  m_responsePtr = 0;
}


bool IECStringDevice::responseBufferEmpty() const
{
  return m_response[m_responsePtr]==0;
}


uint8_t IECStringDevice::toPETSCII(uint8_t data) const
{
  if( data>=65 && data<=90 )
    data += 32;
  else if( data>=97 && data<=122 )
    data -= 32;
  
  return data;
}


uint8_t IECStringDevice::fromPETSCII(uint8_t data) const
{
  if( data==0xFF )
    data = '~';
  else if( data>=192 ) 
    data -= 96;

  if( data>=65 && data<=90 )
    data += 32;
  else if( data>=97 && data<=122 )
    data -= 32;

  return data;
}


void IECStringDevice::task()
{
  // a command has been received => repeatedkly call processCommand()
  // until it returns "true" (or getCommand() is called)
  if( m_commandReceived && processCommand(m_command) )
    {
      m_commandReceived = false;
      m_commandLen = 0;
    }
}
