#include "Button.h"
#include "App.h"
#include "MusicPlayer.h"
#include "print.h"
#include "sioTinyTimber.h"

/* External Objects */
extern SysIO sio0;
extern MusicPlayer musicPlayer;
extern App app;

/* ==========================================================================
 * Button Press Handling
 * ========================================================================== */

int handleButtonPress(Button *self, int UNUSED) {
  Time timeSinceLast = T_SAMPLE(&self->trigTimer);

  // Ignore presses that are too close together (debounce)
  if (timeSinceLast < MSEC(100)) {
    return 0;
  }

  T_RESET(&self->trigTimer);

  // Change trigger to detect release
  SIO_TRIG(&sio0, 1);
  sio0.meth = (Method)handleButtonRelease;

  // Schedule mode entry tasks
  self->resetTask = AFTER(SEC(2), self, enterResetFailMode, NULL);
  self->conductorTask = AFTER(SEC(5), self, enterConductorMode, NULL);

  return 0;
}

/* ==========================================================================
 * Button Release Handling
 * ========================================================================== */

int handleButtonRelease(Button *self, int UNUSED) {
  switch (self->mode) {
  case 0: // Momentary press - toggle mute
    ABORT(self->resetTask);
    ABORT(self->conductorTask);
    ASYNC(&musicPlayer, toggleLedMute, NULL);
    break;

  case 1: // Hold 2-5 sec - reset tempo and key
    ABORT(self->conductorTask);
    switch (SYNC(&app, hasConductorRole, NULL)) {
    case 0: // Not conductor - Failure mode
      // FAILURE MODE CODE LATER
      ASYNC(&app, failureMode, NULL);
      break;
    case 1: // Conductor - reset tempo and key
      ASYNC(&musicPlayer, setTempoBpm, 120);
      ASYNC(&musicPlayer, setKeyOffset, 0);
      ASYNC(&app, sendResetCommand, 0);
      break;
    }
    break;

  case 2: // Hold >5 sec - claim conductor
    if (SYNC(&app, hasConductorRole, NULL)) {
      break;
    }
    ASYNC(&app, sendConductorClaim, 0);
    break;
  }

  // Reset state
  self->mode = 0;
  SIO_TRIG(&sio0, 0);
  sio0.meth = (Method)handleButtonPress;

  return 0;
}

/* ==========================================================================
 * Mode Transition Handlers
 * ========================================================================== */

int enterConductorMode(Button *self, int UNUSED) {
  self->mode = 2;
  print("Entered claim conductor mode\n");
  return 0;
}

int enterResetFailMode(Button *self, int UNUSED) {
  self->mode = 1;
  print("Entered reset tempo and key mode/failure mode\n");
  return 0;
}
