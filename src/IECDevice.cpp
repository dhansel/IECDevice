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

#include "IECDevice.h"

#if defined(__AVR__)

// ---------------- Arduino 8-bit ATMega (UNO/Mega/Mini/Micro/Leonardo...)

#if defined(__AVR_ATmega32U4__)
// Atmega32U4 does not have a second 8-bit timer (first one is used by Arduino millis())
// => use lower 8 bit of 16-bit timer 1
#define timer_init()         { TCCR1A=0; TCCR1B=0; }
#define timer_reset()        TCNT1L=0
#define timer_start()        TCCR1B |= bit(CS11)
#define timer_stop()         TCCR1B &= ~bit(CS11)
#define timer_wait_until(us) while( TCNT1L < ((byte) (2*(us))) )
#else
// use 8-bit timer 2 with /8 prescaler
#define timer_init()         { TCCR2A=0; TCCR2B=0; }
#define timer_reset()        TCNT2=0
#define timer_start()        TCCR2B |= bit(CS21)
#define timer_stop()         TCCR2B &= ~bit(CS21)
#define timer_wait_until(us) while( TCNT2 < ((byte) (2*(us))) )
#endif

//#define JDEBUGI() DDRC |= 0x02; PORTC &= ~0x02
//#define JDEBUG0() PORTC&=~0x02
//#define JDEBUG1() PORTC|=0x02
#define JDEBUGI()
#define JDEBUG0()
#define JDEBUG1()

#else

// ---------------- other (32-bit) platforms

#if defined(__SAM3X8E__)
#define portModeRegister(port) 0
#endif

static unsigned long timer_start_us;
#define timer_init()         while(0)
#define timer_reset()        while(0)
#define timer_start()        timer_start_us = micros()
#define timer_stop()         while(0)
#define timer_wait_until(us) while( (micros()-timer_start_us) < ((int) (us+0.5)) )

//#define JDEBUGI() pinMode(IO4, OUTPUT)
//#define JDEBUG0() GPIO.out_w1tc = bit(4)
//#define JDEBUG1() GPIO.out_w1ts = bit(4)
//#define JDEBUGI() pinMode(2, OUTPUT)
//#define JDEBUG0() digitalWriteFast(2, LOW);
//#define JDEBUG1() digitalWriteFast(2, HIGH);
#define JDEBUGI()
#define JDEBUG0()
#define JDEBUG1()

#endif

// if we have a "digitalWriteFast()" function then use it, otherwise use digitalWrite()
#ifndef digitalWriteFast
#define digitalWriteFast digitalWrite
#endif

// if we have a "digitalReadFast()" function then use it, otherwise use digitalRead()
#ifndef digitalReadFast
#define digitalReadFast digitalRead
#endif

// use faster gpio_* functions to set pin mode on RP2040
#ifndef pinModeFast
#ifdef ARDUINO_ARCH_RP2040
#define pinModeFast(pin, m) if( m==OUTPUT ) { gpio_set_dir(pin, true); } else { gpio_set_dir(pin, false); gpio_pull_up(pin); }
#else
#define pinModeFast pinMode
#endif
#endif

// -----------------------------------------------------------------------------------------


#define P_ATN        0x80
#define P_LISTENING  0x40
#define P_TALKING    0x20
#define P_DONE       0x10
#define P_RESET      0x08
#define P_JIFFY      0x04
#define P_JIFFYBLOCK 0x02


IECDevice *IECDevice::s_iecdevice = NULL;


void IECDevice::writePinCLK(bool v)
{
  // emulate open collector behavior: switch pin to INPUT for HIGH
  // switch pin to output for LOW
#ifdef IOREG_TYPE
  if( v ) 
    { 
#if defined(__SAM3X8E__)
      digitalPinToPort(m_pinCLK)->PIO_OER |= m_bitCLK; // switch to OUTPUT mode
#else
      *m_regCLKmode  &= ~m_bitCLK; // switch to INPUT mode
#endif
      *m_regCLKwrite |=  m_bitCLK; // enable pull-up resistor
    } 
  else 
    {  
      *m_regCLKwrite &= ~m_bitCLK; // set to output "0"
#if defined(__SAM3X8E__)
      digitalPinToPort(m_pinCLK)->PIO_OER |= m_bitCLK; // switch to OUTPUT mode
#else
      *m_regCLKmode  |=  m_bitCLK; // switch to OUTPUT mode
#endif
    }
#else
  if( v )
    {
      pinModeFast(m_pinCLK, INPUT_PULLUP);
    }
  else
    {
      digitalWriteFast(m_pinCLK, LOW);
      pinModeFast(m_pinCLK, OUTPUT);
    }
#endif
}


void IECDevice::writePinDATA(bool v)
{
  // emulate open collector behavior: switch pin to INPUT for HIGH
  // switch pin to output for LOW
#ifdef IOREG_TYPE
  if( v ) 
    { 
#if defined(__SAM3X8E__)
      digitalPinToPort(m_pinDATA)->PIO_ODR |= m_bitDATA; // switch to INPUT mode
#else
      *m_regDATAmode  &= ~m_bitDATA; // switch to INPUT mode
#endif
      *m_regDATAwrite |=  m_bitDATA; // enable pull-up resistor
    }
  else 
    {  
      *m_regDATAwrite &= ~m_bitDATA; // set to output "0"
#if defined(__SAM3X8E__)
      digitalPinToPort(m_pinDATA)->PIO_OER |= m_bitDATA; // switch to OUTPUT mode
#else
      *m_regDATAmode  |=  m_bitDATA; // switch to OUTPUT 1mode
#endif
    }
#else
  if( v )
    {
      pinModeFast(m_pinDATA, INPUT_PULLUP);
    }
  else
    {
      digitalWriteFast(m_pinDATA, LOW);
      pinModeFast(m_pinDATA, OUTPUT);
    }
#endif
}


void IECDevice::writePinCTRL(bool v)
{
  if( m_pinCTRL!=0xFF )
    digitalWriteFast(m_pinCTRL, v);
}


bool IECDevice::readPinATN()
{
#ifdef IOREG_TYPE
  return (*m_regATNread & m_bitATN)!=0;
#else
  return digitalReadFast(m_pinATN);
#endif
}


bool IECDevice::readPinCLK()
{
#ifdef IOREG_TYPE
  return (*m_regCLKread & m_bitCLK)!=0;
#else
  return digitalReadFast(m_pinCLK);
#endif
}


bool IECDevice::readPinDATA()
{
#ifdef IOREG_TYPE
  return (*m_regDATAread & m_bitDATA)!=0;
#else
  return digitalReadFast(m_pinDATA);
#endif
}


bool IECDevice::readPinRESET()
{
  if( m_pinRESET==0xFF ) return true;
#ifdef IOREG_TYPE
  return (*m_regRESETread & m_bitRESET)!=0;
#else
  return digitalReadFast(m_pinRESET);
#endif
}


bool IECDevice::waitTimeoutFrom(uint32_t start, uint16_t timeout)
{
  while( (micros()-start)<timeout )
    if( (m_flags & P_ATN)==0 && !readPinATN() )
      return false;
  
  return true;
}


bool IECDevice::waitTimeout(uint16_t timeout)
{
  return waitTimeoutFrom(micros(), timeout);
}


bool IECDevice::waitPinDATA(bool state)
{
  while( readPinDATA()!=state )
    if( (m_flags & P_ATN)==0 && !readPinATN() )
      return false;

  // DATA LOW can only be properly detected if ATN went HIGH->LOW
  // (m_flags&ATN)==0 and readPinATN()==0)
  // since other devices may have pulled DATA LOW
  return state || (m_flags & P_ATN) || readPinATN();
}


bool IECDevice::waitPinDATA(bool state, uint16_t timeout)
{
  uint32_t start = micros();
  while( readPinDATA()!=state )
    if( ((m_flags & P_ATN)==0 && !readPinATN()) || (uint16_t) (micros()-start)>=timeout )
      return false;

  // DATA LOW can only be properly detected if ATN went HIGH->LOW
  // (m_flags&ATN)==0 and readPinATN()==0)
  // since other devices may have pulled DATA LOW
  return state || (m_flags & P_ATN) || readPinATN();
}


bool IECDevice::waitPinCLK(bool state)
{
  while( readPinCLK()!=state )
    if( (m_flags & P_ATN)==0 && !readPinATN() )
      return false;
  
  return true;
}


bool IECDevice::waitPinCLK(bool state, uint16_t timeout)
{
  uint32_t start = micros();

  while( readPinCLK()!=state )
    if( ((m_flags & P_ATN)==0 && !readPinATN()) || (uint16_t) (micros()-start)>=timeout )
      return false;
  
  return true;
}


IECDevice::IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL)
{
  m_devnr = 0;
  m_flags = 0;
  m_inTask = false;
  m_jiffyBuffer = NULL;
  m_jiffyBufferSize = 0;

  m_pinATN       = pinATN;
  m_pinCLK       = pinCLK;
  m_pinDATA      = pinDATA;
  m_pinRESET     = pinRESET;
  m_pinCTRL      = pinCTRL;

#ifdef IOREG_TYPE
  m_bitRESET     = digitalPinToBitMask(pinRESET);
  m_regRESETread = portInputRegister(digitalPinToPort(pinRESET));
  m_bitATN       = digitalPinToBitMask(pinATN);
  m_regATNread   = portInputRegister(digitalPinToPort(pinATN));
  m_bitCLK       = digitalPinToBitMask(pinCLK);
  m_regCLKread   = portInputRegister(digitalPinToPort(pinCLK));
  m_regCLKwrite  = portOutputRegister(digitalPinToPort(pinCLK));
  m_regCLKmode   = portModeRegister(digitalPinToPort(pinCLK));
  m_bitDATA      = digitalPinToBitMask(pinDATA);
  m_regDATAread  = portInputRegister(digitalPinToPort(pinDATA));
  m_regDATAwrite = portOutputRegister(digitalPinToPort(pinDATA));
  m_regDATAmode  = portModeRegister(digitalPinToPort(pinDATA));
#endif

  m_atnInterrupt = digitalPinToInterrupt(m_pinATN);
}


void IECDevice::begin(byte devnr)
{
  JDEBUGI();

  pinMode(m_pinATN,   INPUT_PULLUP);
  pinMode(m_pinCLK,   INPUT_PULLUP);
  pinMode(m_pinDATA,  INPUT_PULLUP);
  if( m_pinCTRL<0xFF )  pinMode(m_pinCTRL,  OUTPUT);
  if( m_pinRESET<0xFF ) pinMode(m_pinRESET, INPUT_PULLUP);
  m_flags = 0;
  m_devnr = devnr;

  // allow ATN to pull DATA low in hardware
  writePinCTRL(LOW);

  // if the ATN pin is capable of interrupts then use interrupts to detect 
  // ATN requests, otherwise we'll poll the ATN pin in function microTask().
  s_iecdevice = this;
  if( m_atnInterrupt!=NOT_AN_INTERRUPT ) attachInterrupt(m_atnInterrupt, atnInterruptFcn, FALLING);
}


void IECDevice::enableJiffyDosSupport(byte *buffer, byte bufferSize)
{
  m_jiffyBuffer = buffer;
  m_jiffyBufferSize = bufferSize;
}


void IECDevice::atnInterruptFcn() 
{ 
  if( s_iecdevice && !s_iecdevice->m_inTask & ((s_iecdevice->m_flags & P_ATN)==0) )
    s_iecdevice->atnRequest(); 
}


#ifdef SUPPORT_JIFFY

bool IECDevice::receiveJiffyByte(bool canWriteOk)
{
  byte data = 0;
  JDEBUG1();
  timer_init();
  timer_reset();

  noInterrupts(); 

  // signal "ready" by releasing DATA
  writePinDATA(HIGH);

  // wait for either CLK high or ATN low
#ifdef IOREG_TYPE
  while( (*m_regCLKread & m_bitCLK)==0 && (*m_regATNread & m_bitATN)!=0 );
#else
  while( !digitalReadFast(m_pinCLK) && digitalReadFast(m_pinATN) );
#endif

  // start timer (on AVR, lag from CLK high to timer start is between 700...1700ns)
  timer_start();
  JDEBUG0();

  // abort if ATN low
  if( !readPinATN() )
    { JDEBUG0(); interrupts(); return false; }

  // bits 4+5 are set by sender 11 cycles after CLK HIGH (FC51)
  // wait until 14us after CLK
  timer_wait_until(14);
  
  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(4);
  if( !readPinDATA() ) data |= bit(5);
  JDEBUG0();

  // bits 6+7 are set by sender 24 cycles after CLK HIGH (FC5A)
  // wait until 27us after CLK
  timer_wait_until(27);
  
  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(6);
  if( !readPinDATA() ) data |= bit(7);
  JDEBUG0();

  // bits 3+1 are set by sender 35 cycles after CLK HIGH (FC62)
  // wait until 38us after CLK
  timer_wait_until(38);

  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(3);
  if( !readPinDATA() ) data |= bit(1);
  JDEBUG0();

  // bits 2+0 are set by sender 48 cycles after CLK HIGH (FC6B)
  // wait until 51us after CLK
  timer_wait_until(51);

  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(2);
  if( !readPinDATA() ) data |= bit(0);
  JDEBUG0();

  // sender sets EOI status 61 cycles after CLK HIGH (FC76)
  // wait until 64us after CLK
  timer_wait_until(64);

  // if CLK is high at this point then the sender is signaling EOI
  JDEBUG1();
  bool eoi = readPinCLK();

  // acknowledge receipt
  writePinDATA(LOW);

  // sender reads acknowledgement 80 cycles after CLK HIGH (FC82)
  // wait until 83us after CLK
  timer_wait_until(83);

  JDEBUG0();

  interrupts();

  if( canWriteOk )
    {
      // pass received data on to higher layer
      write(data);
    }
  else
    {
      // canWrite() reported an error
      return false;
    }
  
  return true;
}


bool IECDevice::transmitJiffyByte(byte numData)
{
  byte data = numData>0 ? peek() : 0;

  JDEBUG1();
  timer_init();
  timer_reset();

  noInterrupts();

  // signal "READY" by releasing CLK
  writePinCLK(HIGH);
  
  // wait for either DATA high (FBCB) or ATN low
#ifdef IOREG_TYPE
  while( (*m_regDATAread & m_bitDATA)==0 && (*m_regATNread & m_bitATN)!=0 );
#else
  while( !digitalReadFast(m_pinDATA) && digitalReadFast(m_pinATN) );
#endif

  // start timer (on AVR, lag from DATA high to timer start is between 700...1700ns)
  timer_start();
  JDEBUG0();

  // abort if ATN low
  if( !readPinATN() )
    { interrupts(); return false; }

  writePinCLK(data & bit(0));
  writePinDATA(data & bit(1));
  JDEBUG1();
  // bits 0+1 are read by receiver 16 cycles after DATA HIGH (FBD5)

  // wait until 16.5 us after DATA
  timer_wait_until(16.5);
  
  JDEBUG0();
  writePinCLK(data & bit(2));
  writePinDATA(data & bit(3));
  JDEBUG1();
  // bits 2+3 are read by receiver 26 cycles after DATA HIGH (FBDB)

  // wait until 27.5 us after DATA
  timer_wait_until(27.5);

  JDEBUG0();
  writePinCLK(data & bit(4));
  writePinDATA(data & bit(5));
  JDEBUG1();
  // bits 4+5 are read by receiver 37 cycles after DATA HIGH (FBE2)

  // wait until 39 us after DATA
  timer_wait_until(39);

  JDEBUG0();
  writePinCLK(data & bit(6));
  writePinDATA(data & bit(7));
  JDEBUG1();
  // bits 6+7 are read by receiver 48 cycles after DATA HIGH (FBE9)

  // wait until 50 us after DATA
  timer_wait_until(50);
  JDEBUG0();
      
  // numData:
  //   0: no data was available to read (error condition, discard this byte)
  //   1: this was the last byte of data
  //  >1: more data is available after this
  if( numData>1 )
    {
      // CLK=LOW  and DATA=HIGH means "at least one more byte"
      writePinCLK(LOW);
      writePinDATA(HIGH);
    }
  else
    {
      // CLK=HIGH and DATA=LOW  means EOI (this was the last byte)
      // CLK=HIGH and DATA=HIGH means "error"
      writePinCLK(HIGH);
      writePinDATA(numData==0);
    }

  // EOI/error status is read by receiver 59 cycles after DATA HIGH (FBEF)

  interrupts();

  // receiver signals "done" by pulling DATA low (FBF2)
  JDEBUG1();
  if( !waitPinDATA(LOW) ) return false;
  JDEBUG0();

  if( numData>0 )
    {
      // success => discard transmitted byte (was previously read via peek())
      read();
      return true;
    }
  else
    return false;
}


bool IECDevice::transmitJiffyBlock(byte *buffer, byte numBytes)
{
  JDEBUG1();
  timer_init();

  // wait until receiver is not holding DATA low anymore (FB07)
  while( !readPinDATA() )
    if( !readPinATN() )
      { JDEBUG0(); return false; }

  // receiver will be in "new data block" state at this point,
  // waiting for us (FB0C) to release CLK
  
  if( numBytes==0 )
    {
      // nothing to send => signal EOI by keeping DATA high
      // and pulsing CLK high-low
      writePinDATA(HIGH);
      writePinCLK(HIGH);
      if( !waitTimeout(100) ) return false;
      writePinCLK(LOW);
      if( !waitTimeout(100) ) return false;
      JDEBUG0(); 
      return false;
    }

  // signal "ready to send" by pulling DATA low and releasing CLK
  writePinDATA(LOW);
  writePinCLK(HIGH);

  // delay to make sure receiver has seen DATA=LOW - even though receiver 
  // is in a tight loop (at FB0C), a VIC "bad line" may steal 40us.
  if( !waitTimeout(50) ) return false;

  noInterrupts();

  for(byte i=0; i<numBytes; i++)
    {
      byte data = buffer[i];

      // release DATA
      writePinDATA(HIGH);

      // stop and reset timer
      timer_stop();
      timer_reset();

      // signal READY by releasing CLK
      writePinCLK(HIGH);

      // wait for either DATA low (FB51) or ATN low
#ifdef IOREG_TYPE
      while( (*m_regDATAread & m_bitDATA)!=0 && (*m_regATNread & m_bitATN)!=0 );
#else
      while( digitalReadFast(m_pinDATA) && digitalReadFast(m_pinATN) );
#endif

      // start timer (on AVR, lag from DATA low to timer start is between 700...1700ns)
      timer_start();
      JDEBUG0();
      
      // abort if ATN low
      if( !readPinATN() )
        { JDEBUG0(); interrupts(); return false; }

      // receiver expects to see CLK high at 4 cycles after DATA LOW (FB54)
      // wait until 6 us after DATA LOW
      timer_wait_until(6);

      JDEBUG0();
      writePinCLK(data & bit(0));
      writePinDATA(data & bit(1));
      JDEBUG1();
      // bits 0+1 are read by receiver 16 cycles after DATA LOW (FB5D)

      // wait until 17 us after DATA LOW
      timer_wait_until(17);
  
      JDEBUG0();
      writePinCLK(data & bit(2));
      writePinDATA(data & bit(3));
      JDEBUG1();
      // bits 2+3 are read by receiver 26 cycles after DATA LOW (FB63)

      // wait until 27 us after DATA LOW
      timer_wait_until(27);

      JDEBUG0();
      writePinCLK(data & bit(4));
      writePinDATA(data & bit(5));
      JDEBUG1();
      // bits 4+5 are read by receiver 37 cycles after DATA LOW (FB6A)

      // wait until 39 us after DATA LOW
      timer_wait_until(39);

      JDEBUG0();
      writePinCLK(data & bit(6));
      writePinDATA(data & bit(7));
      JDEBUG1();
      // bits 6+7 are read by receiver 48 cycles after DATA LOW (FB71)

      // wait until 50 us after DATA LOW
      timer_wait_until(50);
    }

  // signal "not ready" by pulling CLK LOW
  writePinCLK(LOW);

  // release DATA
  writePinDATA(HIGH);

  interrupts();

  JDEBUG0();

  return true;
}

#endif // !SUPPORT_JIFFY


bool IECDevice::receiveIECByte(bool canWriteOk)
{
  // release DATA ("ready-for-data")
  writePinDATA(HIGH);

  // wait for sender to set CLK=0 ("ready-to-send")
  if( !waitPinCLK(LOW, 200) )
    {
      if( (m_flags & P_ATN)==0 )
        {
          // exit if waitPinCLK returned because of falling edge on ATN
          if( !readPinATN() ) return false;

          // sender did not set CLK=0 within 200us after we set DATA=1
          // => it is signaling EOI (not so if we are under ATN)
          // acknowledge we received it by setting DATA=0 for 80us
          writePinDATA(LOW);
          if( !waitTimeout(80) ) return false;
          writePinDATA(HIGH);
        }

      // keep waiting for CLK=0
      if( !waitPinCLK(LOW) ) return false;
    }

  byte data = 0;
  for(byte i=0; i<8; i++)
    {
      // wait for CLK=1, signaling data is ready
#ifdef SUPPORT_JIFFY
      if( !waitPinCLK(HIGH, 200) )
        {
          if( (m_flags & P_ATN)==0 && !readPinATN() )
            return false;
          else if( (m_flags & P_ATN) && (m_primary==0) && (((data>>1)&0x0F)==m_devnr) && (m_flags&P_JIFFY)==0 && (m_jiffyBufferSize>0) )
            {
              // when sending primary address byte under ATN, host delayed
              // CLK=1 by more than 200us => JiffyDOS protocol detection
              // if we are being addressed then respond that we support 
              // the protocol by pulling DATA low for 80us
              m_flags |= P_JIFFY;
              writePinDATA(LOW);
              if( !waitTimeout(80) ) return false;
              writePinDATA(HIGH);
            }

          // keep waiting fom CLK=1
          if( !waitPinCLK(HIGH) ) return false;
        }
#else
      if( !waitPinCLK(HIGH) ) return false;
#endif
      
      // read DATA bit
      data >>= 1;
      if( readPinDATA() ) data |= 0x80;

      // wait for CLK=0, signaling "data not ready"
      if( !waitPinCLK(LOW) ) return false;
    }

  if( m_flags & P_ATN )
    {
      // We are currently receiving under ATN.  Store first two 
      // bytes received(contain primary and secondary address)
      if( m_primary == 0 )
        m_primary = data;
      else if( m_secondary == 0 )
        m_secondary = data;

      if( (m_primary != 0x3f) && (m_primary != 0x5f) && (((unsigned int) m_primary & 0x1f) != m_devnr) )
        {
          // This is NOT an UNLISTEN (0x3f) or UNTALK (0x5f)
          // command and the primary address is not ours =>
          // Do not acknowledge the frame and stop listening.
          // If all devices on the bus do this, the bus master
          // knows that "Device not present"
          return false;
        }
      else
        {
          // Acknowledge receipt by pulling DATA low
          writePinDATA(LOW);
          return true;
        }
    }
  else if( canWriteOk )
    {
      // acknowledge receipt by pulling DATA low
      writePinDATA(LOW);

      // pass received data on to higher layer
      write(data);
      return true;
    }
  else
    {
      // canWrite() reported an error
      return false;
    }
}


bool IECDevice::transmitIECByte(byte numData)
{
  if( readPinDATA() )
    {
      // "ready to receive" (DATA=1) already signaled before we
      // signaled "ready to send" (CLK=1)
      // observed when reading disk status (e.g. in "COPY190")
      // see code in 1541 ROM disassembly $E919-$E923A
      // (NO EOI handling in this case)
      writePinCLK(HIGH);

      // wait for DATA LOW
      if( !waitPinDATA(LOW) ) return false;
      
      // receiver set DATA to LOW => wait for it to go HIGH again and send next byte 
      // (NO EOI handling in this case)
      writePinCLK(LOW);

      // wait for DATA HIGH (receiver signaling "ready")
      if( !waitPinDATA(HIGH) ) return false;
    }
  else
    {
      // signal "ready-to-send" (CLK=1)
      writePinCLK(HIGH);

      // wait for DATA HIGH ("ready-to-receive")
      //if( !waitTimeout(100) ) return false;
      if( !waitPinDATA(HIGH) ) return false;

      if( numData==1 ) 
        {
          // only this byte left to send => signal EOI by keeping CLK=1
          // wait for receiver to acknowledge EOI by setting DATA=0 then DATA=1
          if( !waitPinDATA(LOW)  ) return false;
          if( !waitPinDATA(HIGH) ) return false;
        }
    }

  // if we have nothing to send then there was some kind of error 
  // => aborting at this stage will signal the error condition to the receiver
  //    (e.g. "File not found" for LOAD)
  if( numData==0 ) return false;

  // signal "data not valid" (CLK=0)
  writePinCLK(LOW);

  // get the data byte from upper layer
  byte data = read();

  // transmit the byte
  for(byte i=0; i<8; i++)
    {
      // signal "data not valid" (CLK=0)
      writePinCLK(LOW);

      // set bit on DATA line
      writePinDATA((data & 1)!=0);

      // hold for 80us
      if( !waitTimeout(80) ) return false;
      
      // signal "data valid" (CLK=1)
      writePinCLK(HIGH);

      // hold for 60us
      if( !waitTimeout(60) ) return false;

      // next bit
      data >>= 1;
    }

  // pull CLK=0 and release DATA=1 to signal "busy"
  writePinCLK(LOW);
  writePinDATA(HIGH);

  // wait for receiver to signal "busy", it must do so
  // within 1ms otherwise error 
  if( !waitPinDATA(LOW, 1000) ) return false;
  
  return true;
}


// called when a falling edge on ATN is detected, either by the pin change
// interrupt handler or by polling within the microTask function
void IECDevice::atnRequest()
{
  // falling edge on ATN detected (bus master addressing all devices)
  m_flags |= P_ATN;
  m_flags &= ~(P_DONE|P_JIFFY|P_JIFFYBLOCK);
  m_primary = 0;
  m_secondary = 0;

  // ignore anything for 100us after ATN falling edge
  m_timeoutStart = micros();

  // release CLK (in case we were holding it LOW before)
  writePinCLK(HIGH);
  
  // set DATA=0 ("I am here").  If nobody on the bus does this within 1ms,
  // busmaster will assume that "Device not present" 
  writePinDATA(LOW);

  // disable the hardware that allows ATN to pull DATA low
  writePinCTRL(HIGH);
}


void IECDevice::task()
{
  // prevent interrupt handler from calling atnRequest()
  m_inTask = true;

  // ------------------ check for activity on RESET pin -------------------

  if( readPinRESET() )
    m_flags |= P_RESET;
  else if( (m_flags & P_RESET)!=0 )
    { 
      // falling edge on RESET pin
      m_flags = 0;
      
      // release CLK and DATA, allow ATN to pull DATA low in hardware
      writePinCLK(HIGH);
      writePinDATA(HIGH);
      writePinCTRL(LOW);

      // call "reset" function in higher classes
      reset(); 
    }

  // ------------------ check for activity on ATN pin -------------------

  if( !(m_flags & P_ATN) && !readPinATN() ) 
    {
      // falling edge on ATN (bus master addressing all devices)
      atnRequest();
    } 
  else if( (m_flags & P_ATN) && readPinATN() )
    {
      // rising edge on ATN (bus master finished addressing all devices)
      m_flags &= ~P_ATN;
      
      // allow ATN to pull DATA low in hardware
      writePinCTRL(LOW);
      
      if (m_primary == 0x20 + m_devnr) 
        {
          // we were told to listen
          listen(m_secondary);
          m_flags &= ~P_TALKING;
          m_flags |= P_LISTENING;
          
          // set DATA=0 ("I am here")
          writePinDATA(LOW);
        } 
      else if (m_primary == 0x40 + m_devnr) 
        {
          // we were told to talk
#ifdef SUPPORT_JIFFY
          if( (m_flags&P_JIFFY)!=0 && m_secondary==0x61 ) 
            { 
              // in JiffyDOS, secondary 0x61 when talking enables "block transfer" mode
              m_secondary = 0x60; 
              m_flags |= P_JIFFYBLOCK; 
            }
#endif        
          talk(m_secondary);
          m_flags &= ~P_LISTENING;
          m_flags |= P_TALKING;

          // wait for bus master to set CLK=1 (and DATA=0) for role reversal
          if( waitPinCLK(HIGH) )
            {
              // now set CLK=0 and DATA=1
              writePinCLK(LOW);
              writePinDATA(HIGH);

              // wait 80us before transmitting first byte of data
              m_timeoutStart = micros();
              m_timeoutDuration = 80;
            }
        }
      else if( (m_primary == 0x3f) && (m_flags & P_LISTENING) )
        {
          // all devices were told to stop listening
          m_flags &= ~P_LISTENING;
          unlisten();
        }
      else if( m_primary == 0x5f && (m_flags & P_TALKING) )
        {
          // all devices were told to stop talking
          untalk();
          m_flags &= ~P_TALKING;
        }

      if( !(m_flags & (P_LISTENING | P_TALKING)) )
        {
          // we're neither listening nor talking => make sure we're not
          // holding the DATA or CLOCK line to 0
          writePinCLK(HIGH);
          writePinDATA(HIGH);
        }
    }

  // ------------------ receiving data -------------------

  if( (m_flags & (P_ATN | P_LISTENING))!=0 && (m_flags & P_DONE)==0 )
    {
      // we are either under ATN or in "listening" mode and not yet done with the transaction

      // check if we can write (also gives devices a chance to
      // execute time-consuming tasks while bus master waits for ready-for-data)
      m_inTask = false;
      int8_t numData = canWrite();
      m_inTask = true;

      if( !(m_flags & P_ATN) && !readPinATN() )
        {
          // a falling edge on ATN happened while we were stuck in "canWrite"
          atnRequest();
        }
      else if( (m_flags & P_ATN) && (micros()-m_timeoutStart)<100 )
        {
          // ignore anything that happens during first 100us after falling
          // edge on ATN (other devices may have been sending and need
          // some time to set CLK=1). m_timeoutStart is set in atnRequest()
        }
#ifdef SUPPORT_JIFFY
      else if( (m_flags&P_JIFFY)!=0 && numData>=0 && !(m_flags & P_ATN) )
        {
          // receiving under JiffyDOS protocol
          if( !receiveJiffyByte(numData>0) )
            {
              // receive failed => release DATA 
              // and stop listening.  This will signal
              // an error condition to the sender
              writePinDATA(HIGH);
              m_flags |= P_DONE;
            }
          }
#endif
      else if( ((m_flags & P_ATN) || numData>=0) && readPinCLK() ) 
        {
          // either under ATN (in which case we always accept data)
          // or canWrite() result was non-negative
          if( !receiveIECByte(numData>0) )
            {
              // receive failed => transaction is done
              writePinDATA(HIGH);
              m_flags |= P_DONE;
            }
        }
    }

  // ------------------ transmitting data -------------------

  if( (m_flags & (P_ATN|P_TALKING|P_DONE))==P_TALKING )
   {
     // we are not under ATN, are in "talking" mode and not done with the transaction

#ifdef SUPPORT_JIFFY
     if( (m_flags & P_JIFFYBLOCK)!=0 )
       {
         // JiffyDOS block transfer mode
         byte numData = read(m_jiffyBuffer, m_jiffyBufferSize);

         // delay to make sure receiver sees our CLK LOW and enters "new data block" state.
         // due possible VIC "bad line" it may take receiver up to 120us after
         // reading bits 6+7 (at FB71) to checking for CLK low (at FB54).
         // If we make it back into transmitJiffyBlock() during that time period
         // then we may already set CLK HIGH again before receiver sees the CLK LOW, 
         // preventing the receiver from going into "new data block" state
         if( !waitTimeoutFrom(m_timeoutStart, 150) || !transmitJiffyBlock(m_jiffyBuffer, numData) )
           {
             // either a transmission error, no more data to send or falling edge on ATN
             m_flags |= P_DONE;
           }
         else
           {
             // remember time when previous transmission finished
             m_timeoutStart = micros();
           }
       }
     else
#endif
       {
         // check if we can read (also gives devices a chance to
         // execute time-consuming tasks while bus master waits for ready-to-send)
        m_inTask = false;
        int8_t numData = canRead();
        m_inTask = true;

        if( !readPinATN() )
          {
            // a falling edge on ATN happened while we were stuck in "canRead"
            atnRequest();
          }
        else if( (micros()-m_timeoutStart)<m_timeoutDuration || numData<0 )
          {
            // either timeout not yet met or canRead() returned a negative value => do nothing
          }
#ifdef SUPPORT_JIFFY
        else if( (m_flags & P_JIFFY)!=0 )
          {
            // JiffyDOS byte-by-byte transfer mode
            if( !transmitJiffyByte(numData) )
              {
                // either a transmission error, no more data to send or falling edge on ATN
                m_flags |= P_DONE;
              }
          }
#endif
        else
          {
            // regular IEC transfer
            if( transmitIECByte(numData) )
              {
                // delay before next transmission ("between bytes time")
                m_timeoutStart = micros();
                m_timeoutDuration = 200;
              }
            else
              {
                // either a transmission error, no more data to send or falling edge on ATN
                m_flags |= P_DONE;
              }
          }
       }
   }

  // allow the interrupt handler to call atnRequest() again
  m_inTask = false;

  // if ATN is low and we don't have P_ATN then we missed the falling edge,
  // make sure to process it before we leave
  if( m_atnInterrupt!=NOT_AN_INTERRUPT && !readPinATN() && !(m_flags & P_ATN) ) { noInterrupts(); atnRequest(); interrupts(); }
}
