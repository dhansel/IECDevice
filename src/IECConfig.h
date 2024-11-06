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
// You should have receikved a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#ifndef IECCONFIG_H
#define IECCONFIG_H

// comment or un-comment these #defines to completely enable/disable support
// for the corresponding fast-load protocols
#define SUPPORT_JIFFY
#define SUPPORT_EPYX
//#define SUPPORT_DOLPHIN

// support Epyx FastLoad sector operations (disk editor, disk copy, file copy)
// if this is enabled then the buffer in the setBuffer() call must have a size of
// at least 256 bytes. Note that the "bufferSize" argument is a byte and therefore
// capped at 255 bytes. Make sure the buffer itself has >=256 bytes and use a 
// bufferSize argument of 255 or less
//#define SUPPORT_EPYX_SECTOROPS

// defines the maximum number of devices that the bus handler will be
// able to support - set to 4 by default but can be increased to up to 30 devices
#define MAX_DEVICES 4

// sets the default size of the fastload buffer. If this is set to 0 then fastload
// protocols can only be used if the IECBusHandler::setBuffer() function is
// called to define the buffer.
#if defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)
#define IEC_DEFAULT_FASTLOAD_BUFFER_SIZE 128
#endif

#endif
