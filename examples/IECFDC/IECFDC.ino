#include "IECFDC.h"

#define PIN_ATN   A5
#define PIN_CLK   A4
#define PIN_DATA  A3
#define PIN_RESET 0xFF
#define PIN_CTRL  A1
#define PIN_LED   A0

static IECFDC IECFDC(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET, PIN_CTRL, PIN_LED);

void setup() 
{
  IECFDC.begin();
}

void loop() 
{
  IECFDC.task();
}
