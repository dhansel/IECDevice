#include "IECFDCMega.h"

#define DEVICE    7

#define PIN_ATN   A3
#define PIN_CLK   A2
#define PIN_DATA  A1
#define PIN_CTRL  A4 // see comment in function IECFDC::task()
#define PIN_LED   A7
#define PIN_RESET 0xFF

static IECFDC IECFDC(PIN_ATN, PIN_CLK, PIN_DATA, PIN_RESET, PIN_CTRL, PIN_LED);

void setup() 
{
  //pinMode(A4, INPUT_PULLUP);
  IECFDC.begin(DEVICE);
}

void loop() 
{
  IECFDC.task();
}
