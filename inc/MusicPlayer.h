#ifndef _MUSICPLAYER_H
#define _MUSICPLAYER_H

#include "TinyTimber.h"
#include <stdint.h>

#define MIN_TEMPO 30
#define MAX_TEMPO 300

extern const int MELODY[32];
extern const int PERIOD[];

//START MUSIC PLAYER CLASS

typedef struct {
  Object super;
  uint16_t tempo;
  int8_t key;
  unsigned char stopped;
} MusicPlayer;

#define initMusicPlayer() {initObject(), 120, 0, 1}

int playTone(MusicPlayer *,int);
int togglePlay(MusicPlayer *, int);
int setTempo(MusicPlayer *,int);
int setKey(MusicPlayer *, int);
int toggleLight(MusicPlayer *, int);
int writeLight(MusicPlayer *, int);

//END MUSIC PLAYER CLASS

int durationToUsec(int, int);
int getPeriod(int);

#endif
