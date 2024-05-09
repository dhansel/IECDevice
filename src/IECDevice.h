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

#define SUPPORT_JIFFY

#if defined(__AVR__)
#define IOREG_TYPE uint8_t
#elif defined(ARDUINO_ARCH_ESP32) || defined(__SAM3X8E__)
#define IOREG_TYPE uint32_t
#endif


class IECDevice
{
 public:
  // pinATN should preferrably be a pin that can handle external interrupts
  // (e.g. 2 or 3 on the Arduino UNO), if not then make sure the task() function
  // gets called at least once evey millisecond, otherwise "device not present" 
  // errors may result
  IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF);

  // must be called once at startup before the first call to "task", devnr
  // is the IEC bus device number that this device should react to
  void begin(byte devnr);

  // task must be called periodically to handle IEC bus communication
  // if the ATN signal is NOT on an interrupt-capable pin then task() must be
  // called at least once every millisecond, otherwise less frequent calls are
  // ok but bus communication will be slower if called less frequently.
  void task();

#ifdef SUPPORT_JIFFY 
  // call this to enable JiffyDOS support for your device. 
  // larger buffers result in improved performance for LOAD operations, 
  // other operations (SAVE, DIR, STATUS) are not affected
  // calling with bufferSize=0 will disable JiffyDOS support
  void enableJiffyDosSupport(byte *buffer, byte bufferSize);
#endif

 protected:
  // called when bus master sends TALK command
  // talk() must return within 1 millisecond
  virtual void talk(byte secondary)   {}

  // called when bus master sends LISTEN command
  // listen() must return within 1 millisecond
  virtual void listen(byte secondary) {}

  // called when bus master sends UNTALK command
  // untalk() must return within 1 millisecond
  virtual void untalk() {}

  // called when bus master sends UNLISTEN command
  // unlisten() must return within 1 millisecond
  virtual void unlisten() {}

  // called before a write() call to determine whether the device
  // is ready to receive data.
  // canWrite() is allowed to take an indefinite amount of time
  // canWrite() should return:
  //  <0 if more time is needed before data can be accepted (call again later), blocks IEC bus
  //   0 if no data can be accepted (error)
  //  >0 if at least one byte of data can be accepted
  virtual int8_t canWrite() { return 0; }

  // called before a read() call to see how many bytes are available to read
  // canRead() is allowed to take an indefinite amount of time
  // canRead() should return:
  //  <0 if more time is needed before we can read (call again later), blocks IEC bus
  //   0 if no data is available to read (error)
  //   1 if one byte of data is available
  //  >1 if more than one byte of data is available
  virtual int8_t canRead() { return 0; }

  // called when the device received data
  // write() will only be called if the last call to canWrite() returned >0
  // write() must return within 1 millisecond
  virtual void write(byte data) {}

  // called when the device is sending data
  // read() will only be called if the last call to canRead() returned >0
  // read() is allowed to take an indefinite amount of time
  virtual byte read() { return 0; }

#ifdef SUPPORT_JIFFY 
  // called when the device is sending data using JiffyDOS byte-by-byte protocol
  // peek() will only be called if the last call to canRead() returned >0
  // peek() should return the next character that will be read with read()
  // peek() is allowed to take an indefinite amount of time
  virtual byte peek() { return 0; }

  // only called when the device is sending data using the JiffyDOS block transfer (LOAD protocol):
  // - should fill the buffer with as much data as possible (up to bufferSize)
  // - must return the number of bytes put into the buffer
  // - if not overloaded, JiffyDOS load performance will be about 3 times slower than otherwise
  // read() is allowed to take an indefinite amount of time
  virtual byte read(byte *buffer, byte bufferSize) { *buffer = read(); return 1; }
#endif

  // called on falling edge of RESET line
  virtual void reset() {}

  byte m_devnr;
  int  m_atnInterrupt;
  byte m_pinATN, m_pinCLK, m_pinDATA, m_pinRESET, m_pinCTRL;

 private:
  inline bool readPinATN();
  inline bool readPinCLK();
  inline bool readPinDATA();
  inline bool readPinRESET();
  inline void writePinCLK(bool v);
  inline void writePinDATA(bool v);
  void writePinCTRL(bool v);
  bool waitTimeoutFrom(uint32_t start, uint16_t timeout);
  bool waitTimeout(uint16_t timeout);
  bool waitPinDATA(bool state);
  bool waitPinDATA(bool state, uint16_t timeout);
  bool waitPinCLK(bool state);
  bool waitPinCLK(bool state, uint16_t timeout);

  void atnRequest();
  bool receiveIECByte(bool canWriteOk);
  bool transmitIECByte(byte numData);

#ifdef IOREG_TYPE
  volatile IOREG_TYPE *m_regATNread, *m_regRESETread;
  volatile IOREG_TYPE *m_regCLKread, *m_regCLKwrite, *m_regCLKmode;
  volatile IOREG_TYPE *m_regDATAread, *m_regDATAwrite, *m_regDATAmode;
  IOREG_TYPE m_bitATN, m_bitCLK, m_bitDATA, m_bitRESET;
#endif

  volatile uint16_t m_timeoutDuration; 
  volatile uint32_t m_timeoutStart;
  volatile bool m_inTask;
  volatile byte m_flags, m_primary, m_secondary;

#ifdef SUPPORT_JIFFY 
  bool receiveJiffyByte(bool canWriteOk);
  bool transmitJiffyByte(byte numData);
  bool transmitJiffyBlock(byte *buffer, byte numBytes);
  byte m_jiffyBufferSize, *m_jiffyBuffer;
#endif

  static IECDevice *s_iecdevice;
  static void atnInterruptFcn();
};

#endif
