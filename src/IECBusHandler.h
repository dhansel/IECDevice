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

#ifndef IECBUSHANDLER_H
#define IECBUSHANDLER_H

#include <Arduino.h>
#include "IECConfig.h"


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

class IECDevice;

class IECBusHandler
{
 public:
  // pinATN should preferrably be a pin that can handle external interrupts
  // (e.g. 2 or 3 on the Arduino UNO), if not then make sure the task() function
  // gets called at least once evey millisecond, otherwise "device not present" 
  // errors may result
  IECBusHandler(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET = 0xFF, byte pinCTRL = 0xFF);

  // must be called once at startup before the first call to "task", devnr
  // is the IEC bus device number that this device should react to
  void begin();

  bool attachDevice(IECDevice *dev);
  bool detachDevice(IECDevice *dev);

  // task must be called periodically to handle IEC bus communication
  // if the ATN signal is NOT on an interrupt-capable pin then task() must be
  // called at least once every millisecond, otherwise less frequent calls are
  // ok but bus communication will be slower if called less frequently.
  void task();

#if (defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)) && !defined(IEC_DEFAULT_FASTLOAD_BUFFER_SIZE)
  // if IEC_DEFAULT_FASTLOAD_BUFFER_SIZE is set to 0 then the buffer space used
  // by fastload protocols can be set dynamically using the setBuffer function.
  void setBuffer(byte *buffer, byte bufferSize);
#endif

#ifdef SUPPORT_JIFFY 
  bool enableJiffyDosSupport(IECDevice *dev, bool enable);
#endif

#ifdef SUPPORT_EPYX
  bool enableEpyxFastLoadSupport(IECDevice *dev, bool enable);
  void epyxLoadRequest(IECDevice *dev);
#endif

#ifdef SUPPORT_DOLPHIN
  // call this BEFORE begin() if you do not want to use the default pins for the DolphinDos cable
  void setDolphinDosPins(byte pinHT, byte pinHR, byte pinD0, byte pinD1, byte pinD2, byte pinD3, 
                         byte pinD4, byte pinD5, byte pinD6, byte pinD7);

  bool enableDolphinDosSupport(IECDevice *dev, bool enable);
  void enableDolphinBurstMode(IECDevice *dev, bool enable);
  void dolphinBurstReceiveRequest(IECDevice *dev);
  void dolphinBurstTransmitRequest(IECDevice *dev);
#endif

  IECDevice *findDevice(byte devnr);
  bool canServeATN();

  IECDevice *m_currentDevice;
  IECDevice *m_devices[MAX_DEVICES];

  byte m_numDevices;
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
  void enableParallelPins();
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
  byte m_bufferSize;
#if IEC_DEFAULT_FASTLOAD_BUFFER_SIZE>0
#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
  byte  m_buffer[256];
#else
  byte  m_buffer[IEC_DEFAULT_FASTLOAD_BUFFER_SIZE];
#endif
#else
  byte *m_buffer;
#endif
#endif

  static IECBusHandler *s_bushandler1,  *s_bushandler2;
  static void atnInterruptFcn1();
  static void atnInterruptFcn2();
};

#endif
