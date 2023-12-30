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

// RP2040 digitalPinToPort() has issues and the RP2040 is
// fast enough anyways to just use digitalRead/digitalWrite
#if defined(ARDUINO_ARCH_RP2040) && defined(digitalPinToPort)
#undef digitalPinToPort
#endif

// compiler should optimize code for performance (not memory)
#pragma GCC optimize ("-O2")

enum {
    P_PRE0 = 0, P_PRE1, P_PRE2, P_PRE3,
    P_READY,
    P_EOI, P_EOIw,
    P_BIT0, P_BIT0w,
    P_BIT1, P_BIT1w,
    P_BIT2, P_BIT2w,
    P_BIT3, P_BIT3w,
    P_BIT4, P_BIT4w,
    P_BIT5, P_BIT5w,
    P_BIT6, P_BIT6w,
    P_BIT7, P_BIT7w,
    P_DONE0, P_DONE1,
    P_FRAMEERR0, P_FRAMEERR1,

    P_TALKING   = 0x20,
    P_LISTENING = 0x40,
    P_ATN       = 0x80
};


IECDevice *IECDevice::s_iecdevice = NULL;

inline void IECDevice::writePinCLK(bool v)
{
  // emulate open collector behavior: switch pin to INPUT for HIGH
  // switch pin to output for LOW
#ifdef digitalPinToPort
  if( v ) 
    { 
      *m_regCLKmode  &= ~m_bitCLK; // switch to INPUT mode
      *m_regCLKwrite |=  m_bitCLK; // enable pull-up resistor
    } 
  else 
    {  
      *m_regCLKwrite &= ~m_bitCLK; // set to output "0"
      *m_regCLKmode  |=  m_bitCLK; // switch to OUTPUT mode
    }
#else
  if( v )
    {
      pinMode(m_pinCLK, INPUT_PULLUP);
    }
  else
    {
      digitalWrite(m_pinCLK, LOW);
      pinMode(m_pinCLK, OUTPUT);
    }
#endif
}


inline void IECDevice::writePinDATA(bool v)
{
  // emulate open collector behavior: switch pin to INPUT for HIGH
  // switch pin to output for LOW
#ifdef digitalPinToPort
  if( v ) 
    { 
      *m_regDATAmode  &= ~m_bitDATA; // switch to INPUT mode
      *m_regDATAwrite |=  m_bitDATA; // enable pull-up resistor
    } 
  else 
    {  
      *m_regDATAwrite &= ~m_bitDATA; // set to output "0"
      *m_regDATAmode  |=  m_bitDATA; // switch to OUTPUT 1mode
    }
#else
  if( v )
    {
      pinMode(m_pinDATA, INPUT_PULLUP);
    }
  else
    {
      digitalWrite(m_pinDATA, LOW);
      pinMode(m_pinDATA, OUTPUT);
    }
#endif
}


inline void IECDevice::writePinCTRL(bool v)
{
  if( m_pinCTRL!=0xFF )
    digitalWrite(m_pinCTRL, v);
}


inline bool IECDevice::readPinATN()
{
#ifdef digitalPinToPort
  return (*m_regATNread & m_bitATN)!=0;
#else
  return digitalRead(m_pinATN);
#endif
}


inline bool IECDevice::readPinCLK()
{
#ifdef digitalPinToPort
  return (*m_regCLKread & m_bitCLK)!=0;
#else
  return digitalRead(m_pinCLK);
#endif
}


inline bool IECDevice::readPinDATA()
{
#ifdef digitalPinToPort
  return (*m_regDATAread & m_bitDATA)!=0;
#else
  return digitalRead(m_pinDATA);
#endif
}


inline bool IECDevice::readPinRESET()
{
  if( m_pinRESET==0xFF ) return true;
#ifdef digitalPinToPort
  return (*m_regRESETread & m_bitRESET)!=0;
#else
  return digitalRead(m_pinRESET);
#endif
}


IECDevice::IECDevice(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL)
{
  m_devnr = 0;
  m_flags = 0;
  m_inMicroTask = false;

  m_pinATN       = pinATN;
  m_pinCLK       = pinCLK;
  m_pinDATA      = pinDATA;
  m_pinRESET     = pinRESET;
  m_pinCTRL      = pinCTRL;

#ifdef digitalPinToPort
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

  // attachInterrupt on RP2040 has issues
#if defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
  m_atnInterrupt = NOT_AN_INTERRUPT;
#else
  m_atnInterrupt = digitalPinToInterrupt(m_pinATN);
#endif
}


void IECDevice::begin(byte devnr)
{
  pinMode(m_pinATN,   INPUT_PULLUP);
  pinMode(m_pinCLK,   INPUT_PULLUP);
  pinMode(m_pinDATA,  INPUT_PULLUP);
  pinMode(m_pinCTRL,  OUTPUT);
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


void IECDevice::atnInterruptFcn() 
{ 
  if( s_iecdevice && !s_iecdevice->m_inMicroTask & ((s_iecdevice->m_flags & P_ATN)==0) )
    s_iecdevice->atnRequest(); 
}


// called when a falling edge on ATN is detected, either by the pin change
// interrupt handler or by polling within the microTask function
void IECDevice::atnRequest()
{
  // falling edge on ATN detected (bus master addressing all devices)
  m_state = P_PRE2;
  m_flags |= P_ATN;
  m_primary = 0;
  m_secondary = 0;

  // ignore anything for 100us after ATN falling edge
  m_timeout = micros() + 100;
  
  // release CLK (in case we were holding it LOW before)
  writePinCLK(HIGH);
  
  // set DATA=0 ("I am here").  If nobody on the bus does this within 1ms,
  // busmaster will assume that "Device not present" 
  writePinDATA(LOW);

  // disable the hardware that allows ATN to pull DATA low
  writePinCTRL(HIGH);
}


void IECDevice::microTask()
{
  if( !readPinRESET() )
    {
      if( m_prevReset ) 
        { 
          m_prevReset = false; 
          m_flags = 0;

          // release CLK and DATA, allow ATN to pull DATA low in hardware
          writePinCLK(HIGH);
          writePinDATA(HIGH);
          writePinCTRL(LOW);
          reset(); 
        }
      return;
    }
  else
    m_prevReset = true;
  
  if (!(m_flags & P_ATN) && !readPinATN() ) {
    /* falling edge on ATN (bus master addressing all devices) */
    atnRequest();
  } else if ((m_flags & P_ATN) && readPinATN()) {
    /* rising edge on ATN (bus master finished addressing all devices) */
    m_flags &= ~P_ATN;

    // allow ATN to pull DATA low in hardware
    writePinCTRL(LOW);
    
    if ((m_primary == 0x20 + m_devnr) || (m_primary == 0x40 + m_devnr)) {
      
      if (m_primary == 0x20 + m_devnr) {
        /* we were told to listen */
        listen(m_secondary);
        m_flags &= ~P_TALKING;
        m_flags |= P_LISTENING;
        m_state = P_PRE2;

        /* set DATA=0 ("I am here") */
        writePinDATA(LOW);
      } else if (m_primary == 0x40 + m_devnr) {
        /* we were told to talk */
        talk(m_secondary);
        m_flags &= ~P_LISTENING;
        m_flags |= P_TALKING;
        m_state = P_PRE0;
      }

    } else if ((m_primary == 0x3f) && (m_flags & P_LISTENING)) {
      /* all devices were told to stop listening */
      m_flags &= ~P_LISTENING;
      unlisten();
    } else if (m_primary == 0x5f && (m_flags & P_TALKING)) {
      /* all devices were told to stop talking */
      untalk();
      m_flags &= ~P_TALKING;
    }

    if (!(m_flags & (P_LISTENING | P_TALKING))) {
      /* we're neither listening nor talking => make sure we're not
         holding the DATA or CLOCK line to 0 */
      writePinCLK(HIGH);
      writePinDATA(HIGH);
    }
  }

  if (m_flags & (P_ATN | P_LISTENING)) {
    /* we are either under ATN or in "listening" mode */
    switch (m_state) {
    case P_PRE2: 
      {
        /* check if we can write (also gives devices a chance to
           execute time-consuming tasks while bus master waits for ready-for-data) */
        m_inMicroTask = false;
        m_numData = canWrite();
        m_inMicroTask = true;

        /* waiting for rising edge on CLK ("ready-to-send") */
        if( !(m_flags & P_ATN) && !readPinATN() ) {
          /* a falling edge on ATN happened while we were stuck in "canWrite" */
          atnRequest();
        } else if( (m_flags & P_ATN) && (micros()<m_timeout) ) {
          /* ignore anything that happens during first 100us after falling
             edge on ATN (other devices may have been sending and need
             some time to set CLK=1) */
        } else if ( ((m_flags & P_ATN) || m_numData>=0) && readPinCLK() ) {
          /* react by setting DATA=1 ("ready-for-data") */
          writePinDATA(HIGH);
          m_timeout = micros() + 200;
          m_state = P_READY;
        }
      }
      break;
    case P_READY:
      if (!readPinCLK()) {
        /* sender set CLK=0, is about to send first bit */
        m_state = P_BIT0;
        m_data  = 0;
      } else if (!(m_flags & P_ATN) && (micros() >= m_timeout)) {
        /* sender did not set CLK=0 within 200us after we set DATA=1
           => it is signaling EOI (not so if we are under ATN)
           acknowledge we received it by setting DATA=0 for 80us */
        writePinDATA(LOW);
        m_state = P_EOI;
        m_timeout = micros() + 80;
      }
      break;
    case P_EOI:
      if (micros() >= m_timeout) {
        /* Set DATA back to 1 and wait for sender to set CLK=0 */
        writePinDATA(HIGH);
        m_state = P_EOIw;
      }
      break;
    case P_EOIw:
      if (!readPinCLK()) {
        /* sender set CLK=0, is about to send first bit */
        m_state = P_BIT0;
        m_data  = 0;
      }
      break;

    case P_BIT0:
    case P_BIT1:
    case P_BIT2:
    case P_BIT3:
    case P_BIT4:
    case P_BIT5:
    case P_BIT6:
    case P_BIT7:
      if (readPinCLK()) {
        /* sender set CLK=1, signaling that the DATA line
           represents a valid bit */
        m_data >>= 1;
        if( readPinDATA() ) m_data |= 0x80;

        /* go to associated P_BIT(n)w state, waiting for sender to
           set CLK=0 */
        m_state++;
      }
      break;
    case P_BIT0w:
    case P_BIT1w:
    case P_BIT2w:
    case P_BIT3w:
    case P_BIT4w:
    case P_BIT5w:
    case P_BIT6w:
      if (!readPinCLK()) {
        /* sender set CLK=0. go to P_BIT(n+1) state to receive
           next bit */
        m_state++;
      }
      break;

    case P_BIT7w:
      {
        if (!readPinCLK()) {

          // data byte was received
          if (m_flags & P_ATN) {
            /* We are currently receiving under ATN.  Store first
               two bytes received
               (contain primary and secondary address) */
            if (m_primary == 0) {
              m_primary = m_data;
            } else if (m_secondary == 0) {
              m_secondary = m_data;
            }

            if (m_primary != 0x3f && m_primary != 0x5f
                && (((unsigned int)m_primary & 0x1f) != m_devnr)) {
              /* This is NOT an UNLISTEN (0x3f) or UNTALK (0x5f)
                 command and the primary address is not ours =>
                 Don't acknowledge the frame and stop listening.
                 If all devices on the bus do this, the bus master
                 knows that "Device not present" */
              m_state = P_DONE0;
            } else {
              /* Acknowledge frame by setting DATA=0 */
              writePinDATA(LOW);

              /* after we've received the secondary address we
                 just wait for ATN to be released, otherwise
                 repeat from P_PRE2 (we know that CLK=0 so
                 no need to go to P_PRE1) */
              m_state = m_secondary==0 ? P_PRE2 : P_DONE0;
            }
          } else if (m_flags & P_LISTENING) {
            if( m_numData>0 ) {
              /* pass received byte on to the higher level */
              write(m_data);

              /* acknowledge the frame by pulling DATA low */
              writePinDATA(LOW);

              /* repeat from P_PRE2 (we know that CLK=0 so no
                 need to go to P_PRE1) */
              m_state = P_PRE2;
            } else {
              /* canWrite() reported an error => release DATA 
                 and stop listening.  This will signal
                 an error condition to the sender */
              writePinDATA(HIGH);
              m_state = P_DONE0;
            }
          }
        }
      }
      break;

    case P_DONE0:
      /* we're just waiting for the busmaster to set ATN back to 1 */
      break;
    }
  } else if (m_flags & P_TALKING) {
    /* we are in "talking" mode */
    switch (m_state) {
    case P_PRE0:
      if ( readPinCLK() ) {
        /* busmaster set CLK=1 (and before that should have set
           DATA=0) we are getting ready for role reversal.
           Set CLK=0, DATA=1 */
        writePinCLK(LOW);
        writePinDATA(HIGH);
        m_state = P_PRE1;
        m_timeout = micros() + 80;
      }
      break;
    case P_PRE1:
      /* wait until timeout has passed */
      if (micros() >= m_timeout ) {
        /* check if we can read (also gives devices a chance to
           execute time-consuming tasks while bus master waits for ready-to-send) */
        m_inMicroTask = false;
        m_numData = canRead();
        m_inMicroTask = true;

        /* if canRead() returned -1 then exit and try again later
           if ATN was asserted during canRead() then exit (ATN will be handled on next invocation)
           otherwise proceed and signal "ready-to-send" */
        if( !readPinATN() ) {
          /* a falling edge on ATN happened while we were stuck in "canRead" */
          atnRequest();
        } else if( (m_flags & P_ATN)==0 && (m_numData>=0) ) {
          if( readPinDATA() )
            {
              // undocumented case? "ready to receive" (DATA=1) already asserted before we
              // assert "ready to send" (CLK=1) => special handling in P_PRE2
              // observed when reading disk status (e.g. in "COPY190")
              // see code in 1541 ROM disassembly $E919-$E923a
              writePinCLK(HIGH);
              m_state = P_PRE2;
            }
          else
            {
              // signal "ready-to-send" (CLK=1)
              writePinCLK(HIGH);
              m_state = P_READY;

              // wait for "ready-to-receive"
              m_timeout = micros() + 100;
            }
        }
      }
      break;
    case P_PRE2:
      if( !readPinDATA() )
        {
          // receiver set DATA to LOW => wait for it to go HIGH again and send next byte 
          // (NO EOI handling in this case)
          writePinCLK(LOW);
          m_state = P_PRE3;
        }
      break;
    case P_PRE3:
      if( readPinDATA() ) {
        // receiver set DATA high again => it is ready to receive
        if( m_numData==0 ) {
          /* There was some kind of error, we have nothing to send.  
             Just stop talking and wait for ATN. */
          m_flags &= ~P_TALKING;
        } else {
          /* Go on to send first bit. */
          m_state = P_BIT0;
          m_timeout = micros() + 40;
          m_data = read();
        }
      }
      break;
    case P_READY:
      if (readPinDATA() && micros()>m_timeout ) {
        /* receiver signaled "ready-for-data" (DATA=1) */
        if( m_numData==0 ) {
          /* There was some kind of error, we have nothing to
             send.  Just stop talking and wait for ATN.
             (This will produce a "File not found" error when
             loading) */
          m_flags &= ~P_TALKING;
        } else {
          if( m_numData==1 ) {
            /* only this byte left to send => signal EOI by
               keeping CLK=1 */
            m_state = P_EOI;
          } else {
            /* at least one more byte left to send after this one.
               Go on to send first bit. */
            m_state = P_BIT0;
            m_timeout = micros() + 40;
            m_data = read();
          }
        }
      }
      break;
    case P_EOI:
      if (!readPinDATA()) {
        /* receiver set DATA=0, first part of acknowledging the
           EOI */
        m_state = P_EOIw;
      }
      break;
    case P_EOIw:
      if (readPinDATA()) {
        /* receiver set DATA=1, final part of acknowledging the EOI.
           Go on to send first bit */
        m_state = P_BIT0;
        m_timeout = micros() + 40;
        m_data = read();
      }
      break;
    case P_BIT0:
    case P_BIT1:
    case P_BIT2:
    case P_BIT3:
    case P_BIT4:
    case P_BIT5:
    case P_BIT6:
    case P_BIT7:
      if (micros() >= m_timeout) {
        /* 60us have passed since we set CLK=1 to signal "data
           valid" for the previous bit.
           Pull CLK=0 and put next bit out on DATA. */
        int bit = 1 << ((m_state - P_BIT0) / 2);
        writePinCLK(LOW);
        writePinDATA((m_data & bit)!=0);

        /* go to associated P_BIT(n)w state */
        //m_timeout = micros() + (bit==1 ? 100 : 80);
        m_timeout = micros() + 100;
        m_state++;
      }
      break;
    case P_BIT0w:
    case P_BIT1w:
    case P_BIT2w:
    case P_BIT3w:
    case P_BIT4w:
    case P_BIT5w:
    case P_BIT6w:
    case P_BIT7w:
      if (micros() >= m_timeout) {
        /* 60us have passed since we pulled CLK=0 and put the
           current bit on DATA.
           set CLK=1, keeping data as it is (this signals "data
           valid" to the receiver) */
        writePinCLK(HIGH);

        /* go to associated P_BIT(n+1) state to send the next bit.
           If this was the final bit then next state is P_DONE0 */
        m_timeout = micros() + 80;
        m_state++;
      }
      break;
    case P_DONE0:
      if (micros() >= m_timeout) {
        /* 60us have passed since we set CLK=1 to signal "data
           valid" for the final bit.
           Pull CLK=0 and set DATA=1.
           This prepares for the receiver acknowledgement. */
        writePinCLK(LOW);
        writePinDATA(HIGH);
        m_timeout = micros() + 1000;
        m_state = P_DONE1;
      }
      break;
    case P_DONE1:
      if (!readPinDATA()) {
        /* Receiver set DATA=0, acknowledging the frame */

        if( m_numData==1 ) {
          /* This was the last byte => stop talking.
             This leaves us waiting for ATN. */
          m_flags &= ~P_TALKING;

          /* Release the CLOCK line to 1 */
          writePinCLK(HIGH);
        } else {
          /* There is at least one more byte to send
             Start over from P_PRE1 */
          m_timeout = micros()+200;
          m_state = P_PRE1;
        }
      } else if (micros() >= m_timeout) {
        /* We didn't receive an acknowledgement within 1ms.
           Set CLOCK=1 and after 100us back to CLOCK=0 */
        writePinCLK(HIGH);
        m_timeout = micros() + 100;
        m_state = P_FRAMEERR0;
      }
      break;
    case P_FRAMEERR0:
      if (micros() >= m_timeout) {
        /* finished 0-1-0 sequence of CLOCK signal to acknowledge
           the frame-error.  Now wait for sender to set DATA=0 so
           we can continue. */
        writePinCLK(LOW);
        m_state = P_FRAMEERR1;
      }
      break;
    case P_FRAMEERR1:
      if (!readPinDATA()) {
        /* sender set DATA=0, we can retry to send the byte */
        m_timeout = micros();
        m_state = P_PRE1;
      }
      break;
    }
  }
}


void IECDevice::task()
{
  // while we are executing microTask we poll for the ATN signal edge,
  // so make sure the interrupt handler does not call atnRequest()
  m_inMicroTask = true;

  // keep repeating the micro task until we are at a stable point:
  // - neither LISTENING nor TALKING nor under ATN
  // - in LISTENING or ATN mode and bus master is waiting for our ready-for-data
  // - in TALKING mode and bus master is waiting for our ready-to-send
  do
    { 
      // do not place Serial.write() debug output here (causes timing errors during receive)
      microTask(); 
    }
  while( ((m_flags&(P_LISTENING|P_ATN))!=0 && (m_state!=P_PRE2)) || ((m_flags&(P_TALKING|P_ATN))==P_TALKING && (m_state!=P_PRE1)) );

  // allow the interrupt handler to call atnRequest() again
  m_inMicroTask = false;

  // if ATN is low and we don't have P_ATN then we missed the falling edge,
  // make sure to process it before we leave
  if( m_atnInterrupt!=NOT_AN_INTERRUPT && !readPinATN() && !(m_flags & P_ATN) ) { noInterrupts(); atnRequest(); interrupts(); }
}
