#include "ToneGenerator.h"
#include "print.h"


int generateTone(ToneGenerator *self, int period) {
  if (self->stopped) {
    self->stopped = 0;
    return 0;
  }
  SEND(USEC(period), USEC(100), self, generateTone, period);
  if (self->muted) {
    return 0;
  }
  int current = READ_REG(DAC->DHR8R2);
  if (current == 0) {
    WRITE_REG(DAC->DHR8R2, self->volume);
  } else {
    WRITE_REG(DAC->DHR8R2, 0);
  }
  return 0;
}

int stopTone(ToneGenerator *self, int unused) {
  self->stopped = 1;
  return 0;
}

int getVolume(ToneGenerator *self, int unused) {
  return self->volume;
}

int setVolume(ToneGenerator *self, int volume) {
  if (volume < MIN_VOLUME) {
    self->volume = MIN_VOLUME;
  } else if (volume > MAX_VOLUME) {
    self->volume = MAX_VOLUME;
  } else {
    self->volume = volume;
  }
  print("Set volume to: %d\n", self->volume);
  return 0;
}

int toggleMute(ToneGenerator *self, int unused) {
  self->muted ^= 1;
  return 0;
}
