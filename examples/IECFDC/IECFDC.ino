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

#include "IECFDC.h"
#include "IECBusHandler.h"

#define DEVICE    9

#define PIN_ATN   A5
#define PIN_CLK   A4
#define PIN_DATA  A3
#define PIN_RESET 0xFF
#define PIN_CTRL  A1
#define PIN_LED   A0


#ifndef __AVR_ATmega328P__
#error "This sketch is meant only for use on an Arduino Uno"
#endif

#if defined(IEC_FP_EPYX) || defined(IEC_FP_FC3) || defined(IEC_FP_AR6) || defined(IEC_FP_DOLPHIN) || defined(IEC_FP_SPEEDDOS)
#error "Not enough program space on Arduino UNO to support anything but JiffyDos - comment out all '#define IEC_FP_*' except '#define_IEC_FP_JIFFY' in file IECConfig.h"
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
