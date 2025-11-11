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
#include <IECBusHandler.h>
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
  IECBasicSD(uint8_t devnum);

 protected:
  virtual void begin();
  virtual void reset();

  virtual bool open(uint8_t channel, const char *name, uint8_t nameLen);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual void close(uint8_t channel);

 private:
  SdFat  m_sd;
  SdFile m_file;
};


IECBasicSD::IECBasicSD(uint8_t devnum) : IECFileDevice(devnum)
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
  IECFileDevice::begin();
}


bool IECBasicSD::open(uint8_t channel, const char *name, uint8_t nameLen)
{
  // open file for reading or writing. Use channel number to determine
  // whether to read (channel 0) or write (channel 1). These channel numbers
  // correspond to the channels used by the C64 LOAD and SAVE commands.
  return m_file.open(name, channel==0 ? O_RDONLY : (O_WRONLY | O_CREAT));
}


uint8_t IECBasicSD::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  // read up to bufferSize bytes from the file opened before, return the number
  // of bytes read or 0 if the file is not open (i.e. an error occurred during open)
  return m_file.isOpen() ? m_file.read(buffer, bufferSize) : 0;
}


uint8_t IECBasicSD::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  // writhe bufferSize bytes to the file opened before, return the number
  // of bytes written or 0 if the file is not open (i.e. an error occurred during open)
  return m_file.isOpen() ? m_file.write(buffer, bufferSize) : 0;
}


void IECBasicSD::close(uint8_t channel)
{
  m_file.close();
}


void IECBasicSD::reset()
{
  m_file.close();
  IECFileDevice::reset();
}


// -----------------------------------------------------------------------------


IECBasicSD iecSD(DEVICE_NUMBER);
IECBusHandler iecBus(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET);

void setup()
{
  // attach the SD device to the bus
  iecBus.attachDevice(&iecSD);

  // initialize bus
  iecBus.begin();
}


void loop()
{
  // handle IEC bus communication (this will call the read and write functions above)
  iecBus.task();
}
