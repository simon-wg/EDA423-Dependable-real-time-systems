#ifndef _MUSICPLAYER_H
#define _MUSICPLAYER_H

#include "TinyTimber.h"
#include <stdint.h>

#define MIN_TEMPO 30
#define MAX_TEMPO 300

extern const int MELODY[32];
extern const int PERIOD[];

/*
 * MusicPlayer Class Definition
 */

typedef struct {
  Object super;
  uint16_t tempo;
  int8_t key;
  unsigned char stopped;
  unsigned char currentlyPlaying;
} MusicPlayer;

#define initMusicPlayer() {initObject(), 120, 0, 1, 0}

/*
 * MusicPlayer Class Methods
 */

int playNote(MusicPlayer *, int);
int startPlayback(MusicPlayer *, int);
int stopPlayback(MusicPlayer *, int);
int setCurrentlyPlaying(MusicPlayer *, int);
int getPlayingState(MusicPlayer *, int);
int getTempo(MusicPlayer *, int);
int getKey(MusicPlayer *, int);
int getNoteDuration(MusicPlayer *, int);
int getRawDuration(MusicPlayer *, int);
int setTempoBpm(MusicPlayer *, int);
int setKeyOffset(MusicPlayer *, int);

/*
 * Helper Functions
 */

int calculateNoteDuration(int, int);
int calculatePeriod(int, int);

#endif
