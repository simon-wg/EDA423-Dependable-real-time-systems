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
  stopWatchdog(self, 0);
  self->checkTimeoutTask = AFTER(MSEC(100), self, checkTimeout, NULL);
  return 0;
}

int notifyWatchdog(Watchdog *self, int nodeId) {
  if (SYNC(&app, getExpectedNodeForNote, self->currentNote) == nodeId)
    T_RESET(&self->heartBeatTimer);
  return 0;
}

int checkTimeout(Watchdog *self, int unused) {
  if (T_SAMPLE(&self->heartBeatTimer) >= MSEC(200)) {
    int failedNode = SYNC(&app, getExpectedNodeForNote, self->currentNote);
    print("Node %d failed to send heartbeat in time. Assuming failure.\n",
          failedNode);
    SYNC(&app, deleteNode, failedNode);
    T_RESET(&self->heartBeatTimer);

    int currentConductor = SYNC(&app, getCurrentConductor, NULL);
    if (failedNode == currentConductor) {
      ASYNC(&app, sendConductorClaim, NULL);
    }

    ASYNC(&app, sendNote, self->currentNote & (failedNode << 8));
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
