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

#include <Arduino.h>
#include <IECFileDevice.h>
#include <SdFat.h>

#define DEVICE_NUMBER 9

#if defined(__AVR__) || defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)

#define PIN_ATN    3
#define PIN_CLK    4
#define PIN_DATA   5
#define PIN_RESET  6
#define PIN_SPI_CS 10

#elif defined(ARDUINO_ARCH_RP2040)

#define PIN_ATN    2
#define PIN_CLK    3
#define PIN_DATA   4
#define PIN_RESET  5
#define PIN_SPI_CS 17

#elif defined(ARDUINO_ARCH_ESP32)

#define PIN_ATN    34
#define PIN_CLK    32
#define PIN_DATA   33
#define PIN_RESET  35
#define PIN_SPI_CS SS

#endif


// -----------------------------------------------------------------------------
// IECBasicSD class implements a very basic SD card reader
// -----------------------------------------------------------------------------

class IECBasicSD : public IECFileDevice
{
 public: 
  IECBasicSD();
  void begin();

 protected:
  virtual void open(byte device, byte channel, const char *name);
  virtual byte read(byte device, byte channel, byte *buffer, byte bufferSize);
  virtual byte write(byte device, byte channel, byte *buffer, byte bufferSize);
  virtual void close(byte device, byte channel);
  virtual void reset();

 private:
  SdFat  m_sd;
  SdFile m_file;
};


IECBasicSD::IECBasicSD() : IECFileDevice(PIN_ATN, PIN_CLK, PIN_DATA)
{
}


void IECBasicSD::begin()
{
  // initialize SD card (note: Pi Pico crashes if m_sd.begin() is called from within IECBasicSD constructor)
#if defined(ARDUINO_ARCH_ESP32)
  SPI.begin(SCK, MISO, MOSI, PIN_SPI_CS);
#endif
#if defined(__SAM3X8E__) || defined(ARDUINO_ARCH_ESP32)
  m_sd.begin(PIN_SPI_CS, SD_SCK_MHZ(8));
#else
  m_sd.begin(PIN_SPI_CS);
#endif

  // initialize IEC bus
  IECFileDevice::begin(DEVICE_NUMBER);
}


void IECBasicSD::open(byte device, byte channel, const char *name)
{
  // open file for reading or writing. Use channel number to determine
  // whether to read (channel 0) or write (channel 1). These channel numbers
  // correspond to the channels used by the C64 LOAD and SAVE commands.
  m_file.open(name, channel==0 ? O_RDONLY : (O_WRONLY | O_CREAT));
}


byte IECBasicSD::read(byte device, byte channel, byte *buffer, byte bufferSize)
{
  // read up to bufferSize bytes from the file opened before, return the number
  // of bytes read or 0 if the file is not open (i.e. an error occurred during open)
  return m_file.isOpen() ? m_file.read(buffer, bufferSize) : 0;
}


byte IECBasicSD::write(byte device, byte channel, byte *buffer, byte bufferSize)
{
  // writhe bufferSize bytes to the file opened before, return the number
  // of bytes written or 0 if the file is not open (i.e. an error occurred during open)
  return m_file.isOpen() ? m_file.write(buffer, bufferSize) : 0;
}


void IECBasicSD::close(byte device, byte channel)
{
  m_file.close(); 
}


void IECBasicSD::reset()
{
  m_file.close();
}


// -----------------------------------------------------------------------------


IECBasicSD iecSD;

void setup()
{
  // initialize device
  iecSD.begin();
}


void loop()
{
  // handle IEC bus communication (this will call the read and write functions above)
  iecSD.task();
}
