#ifndef _MUSIC_H
#define _MUSIC_H

#include "TinyTimber.h"
#include <stdint.h>

#define MAX_VOLUME 20
#define MIN_VOLUME 0

//START TONE GENERATOR CLASS

typedef struct {
  Object super;
  uint8_t volume;
  uint8_t muted;
  uint8_t stopped;
} ToneGenerator;

#define initToneGenerator(v) {initObject(), v, 0, 0};

int generateTone(ToneGenerator *, int);
int stopTone(ToneGenerator *, int);
int getVolume(ToneGenerator *, int);
int setVolume(ToneGenerator *, int);
int toggleMute(ToneGenerator *, int);

//END TONE GENERATOR CLASS

int getPeriod(int);

#endif
