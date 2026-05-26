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

#ifndef IECSTRINGDEVICE_H
#define IECSTRINGDEVICE_H

#include "IECDevice.h"

#define IECSTRINGDEVICE_MAXLEN_COMMAND  80
#define IECSTRINGDEVICE_MAXLEN_RESPONSE 80


class IECStringDevice : public IECDevice
{
 public:

  // if convertPETSCII is set to "true" then received commands will be converted
  // from PETSCII to ASCII and responses will be converted from ASCII to PETSCII
  IECStringDevice(uint8_t devnum, bool convertPETSCII = false);

  // if nothing was received since the last call to getCommand() then getCommand()
  // returns NULL, otherwise it returns a pointer to the received string
  const char *getCommand();

  // set the response to return the next time the bus master asks for data.
  // use standard "printf()" formatting, e.g. setResponse("test %i", n)
  // the response will be cleared after it was sent
  void setResponse(const char *s, ...);

  // set the default response to return if setResponse has not been called
  // when the bus master asks for data
  void setDefaultResponse(const char *s);

  // returns "true" if the response buffer is empty and therefore ready for
  // a new call to setResponse()
  bool responseBufferEmpty() const;

 protected:

  // Override this function to process commands within a derived class. This function will 
  // be called from within the device's "task()" function after a command is received.
  // If processCommand() returns "true" then the command is considered processed and 
  // processCommand() will not be called again until a new command is received. 
  // If it returns "false" then processCommand() will be called for the same command
  // again until either it returns "true" or the "getCommand()" function is called.
  virtual bool processCommand(const char *command) { return false; }

 private:

  virtual int8_t  canRead();
  virtual uint8_t read();
  virtual int8_t  canWrite();
  virtual void    write(uint8_t data, bool eoi);
  virtual void    unlisten();
  virtual void    task();

  uint8_t toPETSCII(uint8_t data) const;
  uint8_t fromPETSCII(uint8_t data) const;

  char m_command[IECSTRINGDEVICE_MAXLEN_COMMAND+1];
  char m_response[IECSTRINGDEVICE_MAXLEN_RESPONSE+1];
  const char *m_defaultResponse;
  uint8_t m_commandLen, m_responsePtr;
  bool m_commandReceived, m_convertPETSCII;
};


#endif
