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
// You should have receikved a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#ifndef IECDEVICE_H
#define IECDEVICE_H

#include <Arduino.h>

// comment or un-comment these #defines to completely enable/disable support
// for the corresponding fast-load protocols
#define SUPPORT_JIFFY
#define SUPPORT_EPYX
//#define SUPPORT_DOLPHIN

// support Epyx FastLoad sector operations (disk editor, disk copy, file copy)
// if this is enabled then the buffer in the setBuffer() call must have a size of
// at least 256 bytes. Note that the "bufferSize" argument is a byte and therefore
// capped 255 bytes. Make sure the buffer itself has >=256 bytes and use a bufferSize
// argument of 255 or less
//#define SUPPORT_EPYX_SECTOROPS

// defines the maximum number of device numbers that the device
// will be able to recognize - this is set to 1 by default but can
// be increased to up to 16 devices, note that increasing this will
// take up more SRAM and flash memory (only an issue on small platforms
// such as Arduino Uno)
#define MAX_DEVICES 1

#if defined(__AVR__)
#define IOREG_TYPE uint8_t
#elif defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)
#define IOREG_TYPE uint16_t
#elif defined(ARDUINO_ARCH_ESP32) || defined(__SAM3X8E__)
#define IOREG_TYPE uint32_t
#endif

#ifndef NOT_AN_INTERRUPT
#define NOT_AN_INTERRUPT -1
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

  // the "begin" call defines one device number that this device will recognize,
  // call "addDeviceNumber" if you want the device to recognize more than one device 
  // number, make sure to set MAX_DEVICES (top of this file) appropriately
  bool addDeviceNumber(byte devnr);

  // task must be called periodically to handle IEC bus communication
  // if the ATN signal is NOT on an interrupt-capable pin then task() must be
  // called at least once every millisecond, otherwise less frequent calls are
  // ok but bus communication will be slower if called less frequently.
  void task();

#ifdef SUPPORT_JIFFY 
  // call this to enable or disable JiffyDOS support for your device.
  // you must call setBuffer() before calling this, otherwise this call will fail
  // and JiffyDos support will not be enabled.
  bool enableJiffyDosSupport(bool enable);
#endif

#ifdef SUPPORT_DOLPHIN 
  // call this to enable or disable DolphinDOS support for your device.
  // you must call setBuffer() before calling this, otherwise this call will fail
  // and DolphinDos support will not be enabled.
  // this function will also fail if any of the pins used for ATN/CLK/DATA
  // are the same as the hardcoded pins for the parallel cable
  bool enableDolphinDosSupport(bool enable);
  
  // call this BEFORE enableDolphinDosSupport if you do not want to use the default
  // pins for the DolphinDos cable
  void setDolphinDosPins(byte pinHT, byte pinHR, byte pinD0, byte pinD1, byte pinD2, byte pinD3, 
                         byte pinD4, byte pinD5, byte pinD6, byte pinD7);
#endif

#ifdef SUPPORT_EPYX
  // call this to enable or disable Expyx FastLoad support for your device. 
  // you must call setBuffer() with a buffer of size at least 32 before calling this, 
  // otherwise this call will fail and Epyx FastLoad support will not be enabled.
  bool enableEpyxFastLoadSupport(bool enable);
#endif

#if defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)
  // call this if you are using either JiffyDos, DolphinDos or Epyx support
  // to save memory, the default buffer is of size 1 which is very
  // inefficient for tranmsissions. A good buffer size is 128.
  void setBuffer(byte *buffer, byte bufferSize);
#endif

 protected:
  // called when bus master sends TALK command
  // talk() must return within 1 millisecond
  virtual void talk(byte devnr, byte secondary)   {}

  // called when bus master sends LISTEN command
  // listen() must return within 1 millisecond
  virtual void listen(byte devnr, byte secondary) {}

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
  virtual int8_t canWrite(byte devnr) { return 0; }

  // called before a read() call to see how many bytes are available to read
  // canRead() is allowed to take an indefinite amount of time
  // canRead() should return:
  //  <0 if more time is needed before we can read (call again later), blocks IEC bus
  //   0 if no data is available to read (error)
  //   1 if one byte of data is available
  //  >1 if more than one byte of data is available
  virtual int8_t canRead(byte devnr) { return 0; }

  // called when the device received data
  // write() will only be called if the last call to canWrite() returned >0
  // write() must return within 1 millisecond
  // the "eoi" parameter will be "true" if sender signaled that this is the last 
  // data byte of a transmission
  virtual void write(byte devnr, byte data, bool eoi) {}

  // called when the device is sending data
  // read() will only be called if the last call to canRead() returned >0
  // read() is allowed to take an indefinite amount of time
  virtual byte read(byte devnr) { return 0; }

#ifdef SUPPORT_JIFFY 
  // called when the device is sending data using JiffyDOS byte-by-byte protocol
  // peek() will only be called if the last call to canRead() returned >0
  // peek() should return the next character that will be read with read()
  // peek() is allowed to take an indefinite amount of time
  virtual byte peek(byte devnr) { return 0; }
#endif

#ifdef SUPPORT_DOLPHIN
  // called when the device is sending data using the DolphinDos burst transfer (SAVE protocol)
  // should write all the data in the buffer and return the number of bytes written
  // returning less than bufferSize signals an error condition
  // the "eoi" parameter will be "true" if sender signaled that this is the final part of the transmission
  // write() is allowed to take an indefinite amount of time
  // the default implementation within IECDevice uses the canWrite() and write(data,eoi) functions,
  // which is not efficient.
  // it is highly recommended to override this function in devices supporting DolphinDos
  virtual byte write(byte devnr, byte *buffer, byte bufferSize, bool eoi);

  void enableDolphinBurstMode(bool enable);
  void dolphinBurstReceiveRequest();
  void dolphinBurstTransmitRequest();
#endif

#if defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)
  // called when the device is sending data using the JiffyDOS block transfer
  // or DolphinDos burst transfer (LOAD protocols)
  // - should fill the buffer with as much data as possible (up to bufferSize)
  // - must return the number of bytes put into the buffer
  // read() is allowed to take an indefinite amount of time
  // the default implementation within IECDevice uses the canRead() and read() functions,
  // which is not efficient.
  // it is highly recommended to override this function in devices supporting JiffyDos or DolphinDos.
  virtual byte read(byte devnr, byte *buffer, byte bufferSize);
#endif

#ifdef SUPPORT_EPYX
  void epyxLoadRequest();

#ifdef SUPPORT_EPYX_SECTOROPS
  // these functions are experimental, they are called when the Epyx Cartridge uses
  // sector read/write operations (disk editor, disk copy or file copy).
  virtual bool epyxReadSector(byte track, byte sector, byte *buffer)  { return false; }
  virtual bool epyxWriteSector(byte track, byte sector, byte *buffer) { return false; }
#endif

#endif

  // called on falling edge of RESET line
  virtual void reset() {}

  bool haveDeviceNumber(byte devnr);
  byte getDeviceIndex(byte devnr);

  byte m_devnr, m_devidx, m_numDevices, m_devices[MAX_DEVICES];
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
  bool waitPinDATA(bool state, uint16_t timeout = 1000);
  bool waitPinCLK(bool state, uint16_t timeout = 1000);

  void atnRequest();
  bool receiveIECByte(bool canWriteOk);
  bool transmitIECByte(byte numData);

  volatile uint16_t m_timeoutDuration; 
  volatile uint32_t m_timeoutStart;
  volatile bool m_inTask;
  volatile byte m_flags, m_primary, m_secondary;
  volatile uint16_t m_sflags;

#ifdef IOREG_TYPE
  volatile IOREG_TYPE *m_regCLKwrite, *m_regCLKmode, *m_regDATAwrite, *m_regDATAmode;
  volatile const IOREG_TYPE *m_regATNread, *m_regCLKread, *m_regDATAread, *m_regRESETread;
  IOREG_TYPE m_bitATN, m_bitCLK, m_bitDATA, m_bitRESET;
#endif

#ifdef SUPPORT_JIFFY 
  bool receiveJiffyByte(bool canWriteOk);
  bool transmitJiffyByte(byte numData);
  bool transmitJiffyBlock(byte *buffer, byte numBytes);
#endif

#ifdef SUPPORT_DOLPHIN
  bool transmitDolphinByte(byte numData);
  bool receiveDolphinByte(bool canWriteOk);
  bool transmitDolphinBurst();
  bool receiveDolphinBurst();

  bool parallelBusHandshakeReceived();
  bool waitParallelBusHandshakeReceived();
  void parallelBusHandshakeTransmit();
  void setParallelBusModeInput();
  void setParallelBusModeOutput();
  byte readParallelData();
  void writeParallelData(byte data);
  bool isDolphinPin(byte pin);

  byte m_pinDolphinHandshakeTransmit;
  byte m_pinDolphinHandshakeReceive;
  byte m_dolphinCtr, m_pinDolphinParallel[8];

#ifdef IOREG_TYPE
  volatile IOREG_TYPE *m_regDolphinHandshakeTransmitMode;
  volatile IOREG_TYPE *m_regDolphinParallelMode[8], *m_regDolphinParallelWrite[8];
  volatile const IOREG_TYPE *m_regDolphinParallelRead[8];
  IOREG_TYPE m_bitDolphinHandshakeTransmit, m_bitDolphinParallel[8];
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega2560__)
  IOREG_TYPE m_handshakeReceivedBit = 0;
#endif
#endif
#endif

#ifdef SUPPORT_EPYX
  bool receiveEpyxByte(byte &data);
  bool transmitEpyxByte(byte data);
  bool receiveEpyxHeader();
  bool transmitEpyxBlock();
#ifdef SUPPORT_EPYX_SECTOROPS
  bool startEpyxSectorCommand(byte command);
  bool finishEpyxSectorCommand();
#endif
#endif
  
#if defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)
  byte m_bufferSize, *m_buffer;
#endif

  static IECDevice *s_iecdevice1,  *s_iecdevice2;
  static void atnInterruptFcn1();
  static void atnInterruptFcn2();
};

#endif
