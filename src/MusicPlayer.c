#include "MusicPlayer.h"
#include "ToneGenerator.h"
#include "sioTinyTimber.h"

extern SysIO sio0;

extern ToneGenerator toneGenerator;

const int MELODY[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4,  5, 7, 7,  9,
                        7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

const int PERIOD[] = {2025, 1911, 1804, 1703, 1607, 1517, 1432, 1351, 1276,
                      1204, 1136, 1073, 1012, 956,  902,  851,  804,  758,
                      716,  676,  638,  602,  568,  536,  506};

// 4 is a quarter note, 2 is a half note, 8 is an eighth note
const char DURATIONS[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 8, 8,
                          8, 8, 4, 4, 8, 8, 8, 8, 4, 4, 4, 4, 2, 4, 4, 2};

int playTone(MusicPlayer *self, int melodyIndex) {
  if (self->stopped) {
    return 0;
  }
  int period = getPeriod(MELODY[melodyIndex] + self->key);
  BEFORE(USEC(100), &toneGenerator, generateTone,
         period); // Start tone generation
  int nextNote = durationToUsec(self->tempo, DURATIONS[melodyIndex]);

  // WE ALSO SCHEDULE LIGHT TOGGLES HERE
  // IF DURATION IS 2, WE SCHEDULE 4 TOGGLES
  // IF DURATION IS 4, WE SCHEDULE 2 TOGGLES
  // IF DURATION IS 8, WE SCHEDULE 1 TOGGLE
  int numToggles = 8 / DURATIONS[melodyIndex];
  int toggleInterval = nextNote / numToggles;
  for (int i = 0; i < numToggles; i++) {
    AFTER(USEC(toggleInterval * i), self, toggleLight, NULL);
  }

  SEND(USEC(nextNote) - MSEC(50), MSEC(50), &toneGenerator, stopTone,
       NULL); // Schedule stopTone
  SEND(USEC(nextNote), USEC(50), self, playTone,
       (melodyIndex + 1) % 32); // Schedule next note
  return 0;
}

int toggleLight(MusicPlayer *self, int UNUSED) {
  SIO_TOGGLE(&sio0);
  return 0;
}

int togglePlay(MusicPlayer *self, int unused) {
  if (self->stopped) {
    self->stopped = 0;
    ASYNC(self, playTone, 0);
  } else
    self->stopped = 1;
  return 0;
}

int setTempo(MusicPlayer *self, int tempo) {
  if (tempo < MIN_TEMPO) {
    tempo = MIN_TEMPO;
  } else if (tempo > MAX_TEMPO) {
    tempo = MAX_TEMPO;
  }
  self->tempo = tempo;
  return 0;
}

int setKey(MusicPlayer *self, int key) {
  if (key < -5) {
    key = -5;
  } else if (key > 5) {
    key = 5;
  }
  self->key = key;
  return 0;
}

int durationToUsec(int tempo, int duration) {
  // BPM is quarter notes per minute
  // We take 4 * 60 seconds (1 minute) and divide
  return 4 * 60 * 1000 * 1000 / tempo / duration;
}

int getPeriod(int semitone) {
  if (semitone < -10 || semitone > 14) {
    return 0;
  }
  return PERIOD[semitone + 10];
}
