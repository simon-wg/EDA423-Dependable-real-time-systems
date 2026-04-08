#include "Button.h"
#include "App.h"
#include "MusicPlayer.h"
#include "print.h"
#include "sioTinyTimber.h"
#include <stdlib.h>

extern SysIO sio0;
extern MusicPlayer musicPlayer;
extern App app;

int buttonPress(Button *self, int UNUSED) {
  Time timeSinceLast = T_SAMPLE(&self->trigTimer); // Get time since last press
  if (timeSinceLast < MSEC(100)) { // Ignore presses that are too close together
    return 0;
  }
  T_RESET(&self->trigTimer); // Reset the timer
  SIO_TRIG(&sio0, 1);        // Change trigger method to trigger on release
  sio0.meth = (Method)buttonRelease; // Set button method to release function
  self->resetTask = AFTER(SEC(2), self, resetTempo, NULL);
  self->conductorTask =
      AFTER(SEC(5), self, claimConductor, NULL); // Schedule tasks
  return 0;
}

int buttonRelease(Button *self, int UNUSED) {
  Time timeSinceLast = T_SAMPLE(&self->trigTimer);
  // Abort tasks if they havent ran
  switch (self->mode) {
  case 0:
    ABORT(self->resetTask);
    ABORT(self->conductorTask);
    ASYNC(&musicPlayer, toggleLightAndMuted, NULL);
    break;
  case 1:
    ABORT(self->conductorTask);
    if (!SYNC(&app, isConductor, NULL))
      break;
    ASYNC(&musicPlayer, setTempo, 120);
    ASYNC(&musicPlayer, setKey, 0);
    ASYNC(&app, canReset, 0);
    break;
  case 2:
    // Send a CAN message to claim conductor status.
    if (SYNC(&app, isConductor, NULL))
      break;
    ASYNC(&app, canClaimConductor, 0);
    break;
  }
  self->mode = 0;
  SIO_TRIG(&sio0, 0);
  sio0.meth = (Method)buttonPress;
  return 0;
}

int claimConductor(Button *self, int UNUSED) {
  self->mode = 2;
  print("Entered claim conductor mode\n");
  return 0;
}

int resetTempo(Button *self, int UNUSED) {
  self->mode = 1;
  print("Entered reset tempo and key mode\n");
  return 0;
}
