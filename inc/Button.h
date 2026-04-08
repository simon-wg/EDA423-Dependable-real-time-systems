#ifndef _BUTTON_H
#define _BUTTON_H

#include "TinyTimber.h"
#include <stdint.h>

typedef struct {
  Object super;
  Timer trigTimer;
  uint8_t mode; // 0: press momentary, 1: press and hold, 2: reset tempo
  Msg resetTask;
  Msg conductorTask;
} Button;

#define initButton() {initObject(), initTimer(), 0, NULL, NULL}

int buttonPress(Button *, int);
int buttonRelease(Button *, int);
int claimConductor(Button *, int);
int resetTempo(Button *, int);

#endif
