#include "Watchdog.h"
#include "App.h"
#include "TinyTimber.h"
#include "print.h"

/* External Objects */
extern App app;

/* ==========================================================================
 * Watchdog Control
 * ========================================================================== */

int resetWatchdog(Watchdog *self, int noteIndex) {
  T_RESET(&self->heartBeatTimer);
  T_RESET(&self->nextNoteTimer);
  self->currentNote = noteIndex;
  ABORT(self->checkTimeoutTask);
  self->checkTimeoutTask = AFTER(MSEC(100), self, checkTimeout, NULL);
  return 0;
}

int notifyWatchdog(Watchdog *self, int nodeId) {
  int currentNodeCount = SYNC(&app, getRegisteredNodeCount, NULL);
  if (SYNC(&app, getNodeOrder, nodeId) ==
      self->currentNote % currentNodeCount) {
    T_RESET(&self->heartBeatTimer);
  }
  return 0;
}

int checkTimeout(Watchdog *self, int unused) {
  if (T_SAMPLE(&self->heartBeatTimer) >= MSEC(20)) {
    print("Node %d failed to send heartbeat in time. Assuming failure.\n",
          self->currentNote);
    // TODO:
    /* Handle node failure (e.g., remove from node list, reassign conductor,
       etc.) FAILURE HANDLING CODE LATER */
    ASYNC(&app, deleteNode, self->currentNote);
    ASYNC(&app, sendNote, self->currentNote);
  }
  self->checkTimeoutTask = AFTER(MSEC(100), self, checkTimeout, NULL);
  return 0;
}

int stopWatchdog(Watchdog *self, int unused) {
  ABORT(self->checkTimeoutTask);
  return 0;
}
