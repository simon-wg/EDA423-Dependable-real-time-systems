#ifndef _TONEGENERATOR_H
#define _TONEGENERATOR_H

#include "TinyTimber.h"
#include <stdint.h>

#define MAX_VOLUME 20
#define MIN_VOLUME 0

/*
 * ToneGenerator Class Definition
 */

typedef struct {
  Object super;
  uint8_t volume;
  uint8_t muted;
  uint8_t stopped;
} ToneGenerator;

#define initToneGenerator(v) {initObject(), v, 0, 0};

/*
 * ToneGenerator Class Methods
 */

int startTone(ToneGenerator *, int);
int stopTone(ToneGenerator *, int);
int getCurrentVolume(ToneGenerator *, int);
int setOutputVolume(ToneGenerator *, int);
int toggleMuteState(ToneGenerator *, int);

#endif
