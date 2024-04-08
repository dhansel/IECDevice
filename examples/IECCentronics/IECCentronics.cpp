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


/*

Arduino   Atmega Register   Direction  Function
RESET     1      PC6        in         RESET
D0        2      PD0        in         Serial RX
D1        3      PD1        out        Serial TX     
D2        4      PD2        in         Printer ACK
D3        5      PD3        in         IEC bus ATN
D4        6      PD4        out        Printer data bit 4
VIn       7                            VCC
GND       8                            GND
-         9      PB6                   Crystal
-         10     PB7                   Crystal
D5        11     PD5        out        Printer data bit 5
D6        12     PD6        out        Printer data bit 6
D7        13     PD7        out        Printer data bit 7
D8        14     PB0        in         IEC bus CLK
D9        15     PB1        in         Shift register DATA
D10       16     PB2        in         Shift register CLK
D11       17     PB3        out        Shift register SH!LD
D12       18     PB4        in/out     IEC bus DATA
D13       19     PB5        in         LED ("ready")
VIn       20                           AVCC
ARef      21                           AREF
GND       22                           GND
A0        23     PC0        out        Printer data bit 0
A1        24     PC1        out        Printer data bit 1
A2        25     PC2        out        Printer data bit 2
A3        26     PC3        out        Printer data bit 3
A4        27     PC4        out        Printer STROBE
A5        28     PC5        out        Printer RESET

Shift register (74HC165)
Input   Pin  Function
D0      11   DIP0
D1      12   DIP1
D2      13   DIP2
D3      14   DIP3
D4       3   Printer SELECT
D5       4   Printer PE (paper end)
D6       5   Printer ERROR
D7       6   Printer BUSY

Compile as Arduino UNO
Fuse bytes (Arduino standard): LOW=0xFF, HIGH=0xDA, EXTENDED=0xFD

*/

#include "IECCentronics.h"
#include "Converter.h"

// printer data bits hardcoded to PORTC 0-3 and PORTD 4-7 in sendByte()
#define PIN_ATN        3
#define PIN_CLK        8
#define PIN_DATA      12
#define PIN_ACK        2
#define PIN_PRIME     A5
#define PIN_STROBE    A4
#define PIN_SR_LATCH  11 // hardcoded to PORTB3 in readShiftRegister() and printerBusy()
#define PIN_SR_CLOCK  10 // hardcoded to PORTB2 in readShiftRegister() and printerBusy()
#define PIN_SR_DATA    9 // hardcoded to PINB1  in readShiftRegister() and printerBusy()
#define PIN_LEDRDY    13

#define DEBUG     0


#if DEBUG>0

static byte debugcount = 0, debugbuf[16];
static const char hex[] = "0123456789ABCDEF";

static void printbuf()
{
#if DEBUG>=3
  Serial.write(' ');
  for(byte i=0; i<debugcount; i++)
    {
      Serial.write(isprint(debugbuf[i]) && debugbuf[i]!=10 && debugbuf[i]!=13 ? debugbuf[i] : '.');
      if( i==7 ) Serial.write(' ');
    }
#endif
}

void DEBUGDATA(byte data)
{
#if DEBUG==1
  Serial.write(data);
  if( data==13 ) Serial.write(10);
  if( data==10 ) Serial.write(13);
#elif DEBUG>=2
  Serial.write(hex[data>>4]);
  Serial.write(hex[data & 0x0f]);
  debugbuf[debugcount++] = data;
  Serial.write(' ');
  
  if( debugcount==8 )
    Serial.write(' ');
  else if( debugcount==16 )
    {
      printbuf();
      Serial.println();
      debugcount = 0;
    }
#endif
}

void DEBUGEND()
{
  if( debugcount>0 )
    {
      for(byte i=debugcount; i<16; i++) { Serial.write(' '); Serial.write(' '); Serial.write(' '); }
      if( debugcount<8 ) Serial.write(' ');
      printbuf();
      Serial.println();
      debugcount = 0;
    }
}

#else
#define DEBUGDATA(d)
#define DEBUGEND()
#endif


static Converter s_defaultConverter;
static IECCentronics *s_instance = NULL;

void printerReadyISR()
{
  s_instance->m_ready = true;
}


IECCentronics::IECCentronics() : IECDevice(PIN_ATN, PIN_CLK, PIN_DATA)
{
  m_mode = 0xFF;
  m_ready = true;
  s_instance = this;
  for(byte i=0; i<8; i++) m_converters[i] = NULL;
}


void IECCentronics::begin()
{
#if DEBUG>0
  Serial.begin(115200);
#endif

  digitalWrite(PIN_STROBE,   HIGH);
  digitalWrite(PIN_PRIME,    HIGH);
  digitalWrite(PIN_LEDRDY,   LOW);
  digitalWrite(PIN_SR_LATCH, LOW);
  digitalWrite(PIN_SR_CLOCK, LOW);
  
  pinMode(PIN_ACK,      INPUT_PULLUP);
  pinMode(PIN_PRIME,    OUTPUT);
  pinMode(PIN_STROBE,   OUTPUT);
  pinMode(PIN_LEDRDY,   OUTPUT);
  pinMode(PIN_SR_LATCH, OUTPUT);
  pinMode(PIN_SR_CLOCK, OUTPUT);
  pinMode(PIN_SR_DATA,  INPUT);

  pinMode( 4, OUTPUT); // D4 (PD4)
  pinMode( 5, OUTPUT); // D5 (PD5)
  pinMode( 6, OUTPUT); // D6 (PD6)
  pinMode( 7, OUTPUT); // D7 (PD7)
  pinMode(A0, OUTPUT); // D0 (PC0)
  pinMode(A1, OUTPUT); // D1 (PC1)
  pinMode(A2, OUTPUT); // D2 (PC2)
  pinMode(A3, OUTPUT); // D3 (PC3)

  m_cmdBufferLen = 0;
  m_cmdBufferPtr = 0;
  m_statusBufferLen = 0;
  m_statusBufferPtr = 0;

  // initialize converters
  for(byte i=0; i<8; i++) 
    if( m_converters[i]==NULL )
      setConverter(i, &s_defaultConverter);

  m_mode = readDIP() & 7;
  m_converters[m_mode]->begin();

  // reset printer
  digitalWrite(PIN_PRIME, LOW);
  delayMicroseconds(10);
  digitalWrite(PIN_PRIME, HIGH);

  // set start-up message
  strncpy_P(m_statusBuffer, PSTR("73,IECCENTRONICS V1.0\r"), BUFSIZE);
  m_statusBufferLen = strlen(m_statusBuffer);

  // set IEC device address (4 or 5)
  IECDevice::begin((readDIP() & 8) ? 5 : 4);

  // set up interrupt handler for printer ACK signal
  attachInterrupt(digitalPinToInterrupt(PIN_ACK), printerReadyISR, FALLING);
}


void IECCentronics::setConverter(byte i, Converter *converter)
{
  if( i<8 ) 
    {
      if( converter==NULL )
        m_converters[i] = &s_defaultConverter;
      else
        {
          m_converters[i] = converter;
          converter->init(this);
        }
    }
}


byte IECCentronics::readDIP()
{
  static byte dip = 0;
  static unsigned long nextReadTime = 0;

  // re-read the shift register every 10 milliseconds
  if( millis() >= nextReadTime )
    {
      dip = (~readShiftRegister()) & 15;
      nextReadTime = millis() + 10;
    }

  return dip;
}


byte IECCentronics::readShiftRegister()
{
  // this implementation takes about 8us to read all 8 bits
  byte res = 0;
  PORTB &= ~0x04;  // set clock low
  PORTB |=  0x08;  // latch inputs

  for(byte i=0; i<8; i++)
    {
      asm (" nop\n nop\n nop\n nop\n"); // 4 cycles = 250ns delay
      res = res * 2;                    // shift result
      if( PINB & 0x02 ) res |= 1;       // read bit
      PORTB |= 0x04; PORTB &= ~0x04;    // pulse clock
    }
  
  PORTB &= ~0x08;  // release inputs
  return res;
}


bool IECCentronics::printerReady()
{
  return m_ready && !printerBusy();
}


bool IECCentronics::printerBusy()
{
  // latch shift register inputs
  PORTB |=  0x08;
  PORTB &= ~0x08;

  // return printer BUSY flag (bit 7 of shift register)
  return (PINB & 0x02)!=0;
}


bool IECCentronics::printerOutOfPaper()
{
  return readShiftRegister() & 0x20;
}


bool IECCentronics::printerError()
{
  return (readShiftRegister() & 0x40)==0;
}


bool IECCentronics::printerSelect()
{
  return readShiftRegister() & 0x10;
}


void IECCentronics::sendByte(byte data)
{
  PORTC = (PORTC & 0xF0) | (data & 0x0F);
  PORTD = (PORTD & 0x0F) | (data & 0xF0);
  digitalWrite(PIN_STROBE, LOW);
  digitalWrite(PIN_STROBE, HIGH);
  m_ready = false;
}


void IECCentronics::talk(byte secondary)
{
  m_channel = secondary & 0x0F;
}


void IECCentronics::listen(byte secondary)
{
  m_channel = secondary & 0x0F;

  if( m_channel==15 )
    m_cmdBufferLen = 0;
  else
    m_converters[m_mode]->setChannel(m_channel);
}


void IECCentronics::unlisten()
{
  if( m_channel==15 )
    {
      // execute command
      if( m_cmdBufferLen>0 )
        {
          m_cmdBuffer[m_cmdBufferLen]=0;
          m_converters[m_mode]->execCommand(m_cmdBuffer);
        }
    }

  DEBUGEND();
}


int8_t IECCentronics::canWrite()
{
  return m_receive.full() ? -1 : 1;
}


void IECCentronics::write(byte data)
{
  m_receive.enqueue(data);

  DEBUGDATA(data);
}


int8_t IECCentronics::canRead()
{
  if( m_channel==15 )
    {
      if( m_statusBufferPtr==m_statusBufferLen )
        {
          m_statusBuffer[0] = 0;
          m_converters[m_mode]->getStatus(m_statusBuffer, BUFSIZE);
          m_statusBufferLen = strlen(m_statusBuffer);
          m_statusBufferPtr = 0;
        }

      return min(m_statusBufferLen-m_statusBufferPtr, 2);
    }
  else
    return 0;
}


byte IECCentronics::read()
{
  return m_statusBuffer[m_statusBufferPtr++];
}


void IECCentronics::task()
{
  // update LED status
  digitalWrite(PIN_LEDRDY, m_converters[m_mode]->ledStatus());

  // check whether the DIP switch mode setting has changed
  if( m_mode != (readDIP() & 3) )
    {
      // notify previous mode handler that it was de-activated
      m_converters[m_mode]->end();

      // set new mode
      m_mode = readDIP() & 7;

      // notify mode handler that it was activated
      m_converters[m_mode]->begin();
    }

  // call converter
  m_converters[m_mode]->convert();

  // if we have data to send and the printer can receive it then send it now
  if( !m_send.empty() && printerReady() )
    sendByte(m_send.dequeue());

  // handle IEC bus communication
  IECDevice::task();
}
