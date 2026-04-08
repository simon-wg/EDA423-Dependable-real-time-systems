#include "MusicPlayer.h"
#include "App.h"
#include "ToneGenerator.h"
#include "print.h"
#include "sioTinyTimber.h"

/* External Objects */
extern App app;
extern SysIO sio0;
extern ToneGenerator toneGenerator;

/* Private Helper Function */
static char shouldPlayNote(int melodyIndex) {
  int order = SYNC(&app, getNodeOrder, NODE_ID);
  int nodes = SYNC(&app, getRegisteredNodeCount, NODE_ID);

  if (order == melodyIndex % nodes) {
    return 1;
  }

  print("Skipping note %d\n", melodyIndex);
  return 0;
}

/* ==========================================================================
 * Melody Data
 * ========================================================================== */

const int MELODY[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4,  5, 7, 7,  9,
                        7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

const int PERIOD[] = {2025, 1911, 1804, 1703, 1607, 1517, 1432, 1351, 1276,
                      1204, 1136, 1073, 1012, 956,  902,  851,  804,  758,
                      716,  676,  638,  602,  568,  536,  506};

// 4 is a quarter note, 2 is a half note, 8 is an eighth note
const char DURATIONS[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 8, 8,
                          8, 8, 4, 4, 8, 8, 8, 8, 4, 4, 4, 4, 2, 4, 4, 2};

/* ==========================================================================
 * Playback Control
 * ========================================================================== */

int playNextNote(MusicPlayer *self, int melodyIndex) {
  if (self->stopped) {
    return 0;
  }

  int period = calculatePeriod(MELODY[melodyIndex], self->key);
  int noteDuration = calculateNoteDuration(self->tempo, DURATIONS[melodyIndex]);

  if (shouldPlayNote(melodyIndex)) {
    BEFORE(USEC(100), &toneGenerator, startTone, period);
  }

  scheduleLedToggle(self, melodyIndex, noteDuration);

  if (shouldPlayNote(melodyIndex)) {
    SEND(USEC(noteDuration) - MSEC(50), MSEC(50), &toneGenerator, stopTone, NULL);
  }

  SEND(USEC(noteDuration), USEC(50), self, playNextNote, (melodyIndex + 1) % 32);
  return 0;
}

int togglePlayback(MusicPlayer *self, int UNUSED) {
  if (self->stopped) {
    self->stopped = 0;
    ASYNC(self, playNextNote, 0);
  } else {
    self->stopped = 1;
  }
  return 0;
}

/* ==========================================================================
 * Music Configuration
 * ========================================================================== */

int setTempoBpm(MusicPlayer *self, int tempo) {
  if (tempo < MIN_TEMPO) {
    tempo = MIN_TEMPO;
  } else if (tempo > MAX_TEMPO) {
    tempo = MAX_TEMPO;
  }
  self->tempo = tempo;
  return 0;
}

int setKeyOffset(MusicPlayer *self, int key) {
  if (key < -5) {
    key = -5;
  } else if (key > 5) {
    key = 5;
  }
  self->key = key;
  return 0;
}

/* ==========================================================================
 * LED Control
 * ========================================================================== */

int toggleLed(MusicPlayer *self, int UNUSED) {
  SIO_TOGGLE(&sio0);
  return 0;
}

int toggleLedMute(MusicPlayer *self, int UNUSED) {
  if (SYNC(&app, hasConductorRole, 0)) {
    return 0;
  }
  int muted = SYNC(&toneGenerator, toggleMuteState, NULL);
  SIO_WRITE(&sio0, muted);
  return 0;
}

void scheduleLedToggle(MusicPlayer *self, int melodyIndex, int noteDuration) {
  if (!SYNC(&app, hasConductorRole, 0)) {
    return;
  }

  int numToggles = 8 / DURATIONS[melodyIndex];
  int toggleInterval = noteDuration / numToggles;

  for (int i = 0; i < numToggles; i++) {
    AFTER(USEC(toggleInterval * i), self, toggleLed, NULL);
  }
}

/* ==========================================================================
 * Calculation Helpers
 * ========================================================================== */

int calculateNoteDuration(int tempo, int duration) {
  // BPM is quarter notes per minute
  // We take 4 * 60 seconds (1 minute) and divide
  return 4 * 60 * 1000 * 1000 / tempo / duration;
}

int calculatePeriod(int baseSemitone, int key) {
  int semitone = baseSemitone + key;
  if (semitone < -10 || semitone > 14) {
    return 0;
  }
  return PERIOD[semitone + 10];
}
