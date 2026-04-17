#ifndef _WATCHDOG_H
#define _WATCHDOG_H

#include "TinyTimber.h"
#include <stdint.h>

/*
 * Watchdog Class Definition
 */

typedef struct {
  Object super;
  Timer heartBeatTimer;
  Timer nextNoteTimer;
  uint16_t currentNote;
  Msg checkTimeoutTask;
} Watchdog;

#define initWatchdog() {initObject(), initTimer(), initTimer(), 0, NULL}

/*
 * Watchdog Class Methods
 */

int resetWatchdog(Watchdog *, int);
int notifyWatchdog(Watchdog *, int);
int checkTimeout(Watchdog *, int);
int stopWatchdog(Watchdog *, int);

#endif
