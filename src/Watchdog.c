#include "Watchdog.h"
#include "App.h"
#include "MusicPlayer.h"
#include "TinyTimber.h"
#include "print.h"

/* External Objects */
extern App app;
extern MusicPlayer musicPlayer;

/* ==========================================================================
 * Watchdog Control
 * ========================================================================== */

int resetWatchdog(Watchdog *self, int noteIndex) {
  T_RESET(&self->heartBeatTimer);
  self->currentNote = noteIndex;
  self->currentNode = SYNC(&app, getExpectedNodeForNote, noteIndex);
  stopWatchdog(self, 0);
  self->checkTimeoutTask = AFTER(MSEC(100), self, checkTimeout, NULL);
  return 0;
}

int notifyWatchdog(Watchdog *self, int nodeId) {
  if (self->currentNode == nodeId)
    T_RESET(&self->heartBeatTimer);
  return 0;
}

int checkTimeout(Watchdog *self, int unused) {
  print("Checking if node %d has timed out for note %d\n", self->currentNode,
        self->currentNote);
  if (T_SAMPLE(&self->heartBeatTimer) >= MSEC(200)) {
    print("Node %d failed to send heartbeat in time. Assuming failure.\n",
          self->currentNode);
    SYNC(&app, deleteNode, self->currentNode);

    int currentConductor = SYNC(&app, getCurrentConductor, NULL);
    if (self->currentNode == currentConductor) {
      ASYNC(&app, sendConductorClaim, NULL);
    }

    ASYNC(&app, sendNodeFailure, self->currentNode);
    ASYNC(&app, sendNote, self->currentNote);
    self->checkTimeoutTask = NULL;
    return 0;
  }
  self->checkTimeoutTask = AFTER(MSEC(100), self, checkTimeout, NULL);
  return 0;
}

int stopWatchdog(Watchdog *self, int unused) {
  if (!self->checkTimeoutTask)
    return 0;
  ABORT(self->checkTimeoutTask);
  self->checkTimeoutTask = NULL;
  return 0;
}
