#include "IECFDC.h"

#define PIN_ATN   A5
#define PIN_CLK   A4
#define PIN_DATA  A3
#define PIN_RESET 0xFF
#define PIN_CTRL  A1
#define PIN_LED   A0

static IECFDC IECFDC(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET, PIN_CTRL, PIN_LED);

#ifdef SUPPORT_DOLPHIN
#error "Not enough program space (or pins) on Arduino UNO to support Dolphin DOS in this sketch - remove '#define SUPPORT_DOLPHIN' in file IECDevice.h"
#endif

#ifdef SUPPORT_EPYX
#error "Not enough program space on Arduino UNO to support Epyx FastLoad in this sketch - remove '#define SUPPORT_EPYX' in file IECDevice.h"
#endif

void setup() 
{
  IECFDC.begin();
}

void loop() 
{
  IECFDC.task();
}
