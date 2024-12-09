// -----------------------------------------------------------------------------
// Copyright (C) 2024 David Hansel
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

#include "IECFDCMega.h"
#include "IECBusHandler.h"

#define DEVICE    7

#define PIN_ATN   A3
#define PIN_CLK   A2
#define PIN_DATA  A1
#define PIN_CTRL  A4 // see comment in function IECFDC::task()
#define PIN_LED   A7
#define PIN_RESET 0xFF

#ifndef __AVR_ATmega2560__
#error "This sketch is meant only for use on an Arduino Mega 2560"
#endif

IECFDC iecFDC(DEVICE, PIN_LED);
IECBusHandler iecBus(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET, PIN_CTRL);


void setup() 
{
  iecBus.attachDevice(&iecFDC);
  iecBus.begin();
}

void loop() 
{
  iecBus.task();
}
