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

#ifndef CONVERTERTANDYMPS801
#define CONVERTERTANDYMPS801

#include "Converter.h"

class ConverterTandyMPS801 : public Converter
{
 public:
  ConverterTandyMPS801();

  virtual void begin();
  virtual void setChannel(byte channel);

  virtual void convert();
  
 private:
  byte m_mode;
  byte m_cmdbytes;
  unsigned short m_dotcolumn;
};


#endif
