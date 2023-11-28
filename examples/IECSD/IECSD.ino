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
  iecsd.begin(7);
}


void loop()
{
  iecsd.task();
}
