#include "MusicPlayer.h"
#include "App.h"
#include "ToneGenerator.h"
#include "print.h"
#include "sioTinyTimber.h"

/* External Objects */
extern App app;
extern SysIO sio0;
extern ToneGenerator toneGenerator;

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

int startPlayback(MusicPlayer *self, int UNUSED) {
  self->stopped = 0;
  return 0;
}

int stopPlayback(MusicPlayer *self, int unused) {
  self->stopped = 1;
  return 0;
}

int playNote(MusicPlayer *self, int melodyIndex) {
  if (self->stopped || self->currentlyPlaying)
    return 0;

  self->currentlyPlaying = 1;
  print("Playing note %d\n", melodyIndex);
  int period = calculatePeriod(MELODY[melodyIndex], self->key);
  int noteDuration = calculateNoteDuration(self->tempo, DURATIONS[melodyIndex]);

  BEFORE(USEC(100), &toneGenerator, startTone, period);
  SEND(USEC(noteDuration) - MSEC(50), USEC(10), self, setCurrentlyPlaying, 0);
  SEND(USEC(noteDuration) - MSEC(50), MSEC(50), &toneGenerator, stopTone, NULL);

  AFTER(USEC(noteDuration), &app, sendNote, (melodyIndex + 1) % 32);
  return 0;
}

int setCurrentlyPlaying(MusicPlayer *self, int playing) {
  self->currentlyPlaying = playing;
  return 0;
}

int getPlayingState(MusicPlayer *self, int unused) { return !self->stopped; }
int getRawDuration(MusicPlayer *self, int note) { return DURATIONS[note % 32]; }
int getNoteDuration(MusicPlayer *self, int note) {
  return calculateNoteDuration(self->tempo, DURATIONS[note % 32]);
}
int getTempo(MusicPlayer *self, int unused) { return self->tempo; }

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
  print("Tempo set to %d BPM\n", tempo);
  return 0;
}

int setKeyOffset(MusicPlayer *self, int key) {
  if (key < -5) {
    key = -5;
  } else if (key > 5) {
    key = 5;
  }
  self->key = key;
  print("Key set to %d\n", key);
  return 0;
}

/* ==========================================================================
 * Calculation Helpers
 * ========================================================================== */

int calculateNoteDuration(int tempo, int duration) {
  // BPM is quarter notes per minute
  // We take 4 * 60 seconds (1 minute) and divide
  // 4 * 60 / tempo gives duration of a quarter note in seconds.
  // Multiply with 1000 * 1000 gives micros
  return 4 * 60 * 1000 * 1000 / tempo / duration;
}

int calculatePeriod(int baseSemitone, int key) {
  int semitone = baseSemitone + key;
  if (semitone < -10 || semitone > 14) {
    return 0;
  }
  return PERIOD[semitone + 10];
}
