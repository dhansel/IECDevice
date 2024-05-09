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

#include "IECSD.h"

// IEC bus device number
#define DEVICE_NUMBER 9

// --- IEC bus connections
//
//       Uno    Mega   Mico   Due   PiPico   ESP32  WT32-ETH01
// ATN   3      3      3      3     GPIO2    IO34   IO35
// CLK   4      4      4      4     GPIO3    IO32   IO14
// DATA  5      5      5      5     GPIO4    IO33   IO15
// RESET 6      6      6      6     GPIO5    IO35   IO39
//
//
// --- SD card module connections (*=use on-board 6-pin SPI header)
//
//       Uno    Mega   Mico   Due   PiPico   ESP32  WT32-ETH01
// SCK   13/*   52/*   15     *     GPIO18   IO18   IO2
// MISO  12/*   50/*   14     *     GPIO16   IO19   IO36
// MOSI  11/*   51/*   16     *     GPIO19   IO23   IO12
// CS    8      8      8      8     GPIO17   IO5    IO4
//
// --- Activity LED connection (*=on-board LED)
//
//       Uno    Mega   Mico   Due   PiPico   ESP32
// LED   A0     13/*   *      13/*  *        IO21   -



#if defined(__AVR__) || defined(__SAM3X8E__)

#define PIN_IEC_ATN    3
#define PIN_IEC_CLK    4
#define PIN_IEC_DATA   5
#define PIN_IEC_RESET  6
#define PIN_SPI_CS     8

#elif defined(ARDUINO_ARCH_RP2040)

#define PIN_IEC_ATN    2
#define PIN_IEC_CLK    3
#define PIN_IEC_DATA   4
#define PIN_IEC_RESET  5
#define PIN_SPI_CS     17

#elif defined(ARDUINO_WT32_ETH01)

#define PIN_IEC_ATN   IO35
#define PIN_IEC_CLK   IO14
#define PIN_IEC_DATA  IO15
#define PIN_IEC_RESET IO39

#define PIN_SPI_CLK   IO2
#define PIN_SPI_MISO  IO36
#define PIN_SPI_MOSI  IO12
#define PIN_SPI_CS    IO4

#elif defined(ARDUINO_ARCH_ESP32)

#include <SPI.h>

#define PIN_IEC_ATN   34
#define PIN_IEC_CLK   32
#define PIN_IEC_DATA  33
#define PIN_IEC_RESET 35

#define PIN_SPI_CLK   SCK
#define PIN_SPI_MISO  MISO
#define PIN_SPI_MOSI  MOSI
#define PIN_SPI_CS    SS

#define PIN_LED       21

#endif

// activity LED pin
#if !defined(PIN_LED)
#if defined(ARDUINO_AVR_UNO)
#define PIN_LED  A0              // on UNO, LED_BUILTIN (pin 13) conflicts with SPI SCK
#elif defined(ARDUINO_AVR_MICRO)
#define PIN_LED  LED_BUILTIN_TX  // use "TX" LED on PRO MICRO
#elif defined(LED_BUILTIN)
#define PIN_LED  LED_BUILTIN     // use built-in LED
#else
#define PIN_LED  0xFF            // no LED
#endif
#endif


IECSD iecsd(PIN_IEC_ATN, PIN_IEC_CLK, PIN_IEC_DATA, PIN_IEC_RESET, PIN_SPI_CS, PIN_LED);


void setup()
{
#if defined(ARDUINO_ARCH_ESP32)
  SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
#endif
  iecsd.begin(DEVICE_NUMBER);
}


void loop()
{
  iecsd.task();
}
