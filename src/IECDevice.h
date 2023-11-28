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

#ifndef IECDEVICE_H
#define IECDEVICE_H

#include <Arduino.h>

class IECDevice
{
 public:
  // pinATN should preferrably be a pin that can handle external interrupts
  // (e.g. 2 or 3 on the Arduino UNO), if not then make sure the task() function
  // gets called at least once evey millisecond, otherwise "device not present" 
  // errors may result
  IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF);

  // must be called once at startup before the first call to "task", devnr
  // is the IEC bus device number that this device should react to
  void begin(byte devnr);

  // task must be called periodically to handle IEC bus communication
  // if the ATN signal is NOT on an interrupt-capable pin then task() must be
  // called at least once every millisecond, otherwise less frequent calls are
  // ok but bus communication will be slower if called less frequently.
  // task() will take at most 5 milliseconds to execute before returning
  void task();

 protected:
  // called when bus master sends OPEN command
  // data received by "write" after this (and terminated by
  // the next UNLISTEN command) identifies the file name of
  // the file to be opened. 
  // open() must return within 1 millisecond
  virtual void open(byte channel)   {}

  // called when bus master sends CLOSE command
  // close() must return within 1 millisecond
  virtual void close(byte channel)  {}

  // called when bus master sends TALK command
  // talk() must return within 1 millisecond
  virtual void talk(byte channel)   {}

  // called when bus master sends LISTEN command
  // listen() must return within 1 millisecond
  virtual void listen(byte channel) {}

  // called when bus master sends UNTALK command
  // untalk() must return within 1 millisecond
  virtual void untalk(byte channel) {}

  // called when bus master sends UNLISTEN command
  // unlisten() must return within 1 millisecond
  virtual void unlisten(byte channel) {}

  // called before a write() call to determine whether the device
  // is ready to receive data.
  // canWrite() is allowed to take an indefinite amount of time
  // canWrite() should return:
  //  <0 if more time is needed before data can be accepted (call again later), blocks IEC bus
  //   0 if no data can be accepted (error)
  //  >0 if at least one byte of data can be accepted
  virtual int8_t canWrite(byte channel) { return 0; }

  // called before a read() call to see how many bytes are available to read
  // canRead() is allowed to take an indefinite amount of time
  // canRead() should return:
  //  <0 if more time is needed before we can read (call again later), blocks IEC bus
  //   0 if no data is available to read (error)
  //   1 if one byte of data is available
  //  >1 if more than one byte of data is available
  virtual int8_t canRead(byte channel) { return 0; }

  // called when the device received data
  // write() will only be called if the last call to canWrite() returned >0
  // write() must return within 1 millisecond
  virtual void write(byte channel, byte data) {}

  // called when the device is sending data
  // read() will only be called if the last call to canRead() returned >0
  // read() is allowed to take an indefinite amount of time
  virtual byte read(byte channel) { return 0; }

  // called on falling edge of RESET line
  virtual void reset() {}

 private:
  inline bool readPinATN();
  inline bool readPinCLK();
  inline bool readPinDATA();
  inline bool readPinRESET();
  inline void writePinCLK(bool v);
  inline void writePinDATA(bool v);

  void microTask();
  void atnRequest();

  byte m_devnr;
  byte m_pinATN, m_pinCLK, m_pinDATA, m_pinRESET;

  volatile uint8_t *m_regATNread, *m_regRESETread;
  volatile uint8_t *m_regCLKread, *m_regCLKwrite, *m_regCLKmode;
  volatile uint8_t *m_regDATAread, *m_regDATAwrite, *m_regDATAmode;
  uint8_t m_bitATN, m_bitCLK, m_bitDATA, m_bitRESET;
  int m_atnInterrupt;

  volatile unsigned long m_timeout;
  volatile bool m_inMicroTask;
  volatile byte m_state, m_flags, m_primary, m_secondary, m_secondary_prev;
  int8_t m_numData;
  byte m_data;
  bool m_prevReset;

  static IECDevice *s_iecdevice;
  static void atnInterruptFcn();
};

#endif
