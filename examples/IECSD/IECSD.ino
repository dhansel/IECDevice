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

#define PIN_ATN   3
#define PIN_CLK   4
#define PIN_DATA  5
#define PIN_RESET 6
#define PIN_LED   A0
#define PIN_CHIPSELECT 8

IECSD iecsd(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET, PIN_CHIPSELECT, PIN_LED);


void setup()
{
  iecsd.begin(9);
}


void loop()
{
  iecsd.task();
}
