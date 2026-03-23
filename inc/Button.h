#ifndef _BUTTON_H
#define _BUTTON_H

#include "TinyTimber.h"
#include <stdint.h>

typedef struct {
  Object super;
  int16_t presses[3];
  uint8_t pressIndex;
  Timer trigTimer;
  uint8_t mode; // 0: press momentary, 1: press and hold, 2: reset tempo
  Msg pahTask;
  Msg resetTask;
} Button;

#define initButton() {initObject(), {}, 0, initTimer(), 0, NULL, NULL}

int buttonPress(Button *, int);
int buttonRelease(Button *, int);
int pressAndHold(Button *, int);
int resetTempo(Button *, int);
int appendPress(Button *, int);
int calculateTempo(int16_t presses[3]);

#endif
