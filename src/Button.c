#include "Button.h"
#include "MusicPlayer.h"
#include "print.h"
#include "sioTinyTimber.h"
#include <stdlib.h>

extern SysIO sio0;
extern MusicPlayer musicPlayer;

int buttonPress(Button *self, int UNUSED) {
  Time timeSinceLast = T_SAMPLE(&self->trigTimer); // Get time since last press
  if (timeSinceLast < MSEC(100)) { // Ignore presses that are too close together
    return 0;
  }
  print("Button pressed, offset: %dms\n",
        timeSinceLast / 100); // 100 ticks per ms

  T_RESET(&self->trigTimer); // Reset the timer
  SIO_TRIG(&sio0, 1);        // Change trigger method to trigger on release
  sio0.meth = (Method)buttonRelease; // Set button method to release function
  self->pahTask = AFTER(SEC(1), self, pressAndHold, NULL); // Schedule tasks
  self->resetTask = AFTER(SEC(2), self, resetTempo, NULL);
  ASYNC(self, appendPress, timeSinceLast / 100);
  return 0;
}

int buttonRelease(Button *self, int UNUSED) {
  Time timeSinceLast = T_SAMPLE(&self->trigTimer);
  // Abort tasks if they havent ran
  if (self->mode < 2) {
    ABORT(self->resetTask);
  }
  if (self->mode < 1) {
    ABORT(self->pahTask);
  }
  print("Button released after %dms\n",
        timeSinceLast / 100); // 100 ticks per ms
  self->mode = 0;
  SIO_TRIG(&sio0, 0);
  sio0.meth = (Method)buttonPress;
  return 0;
}

int pressAndHold(Button *self, int UNUSED) {
  self->mode = 1;
  print("Entered press and hold mode\n");
  return 0;
}

int resetTempo(Button *self, int UNUSED) {
  self->mode = 2;
  print("Resetting tempo to default\n");
  ASYNC(&musicPlayer, setTempo, 120);
  return 0;
}

int appendPress(Button *self, int press) {
  self->presses[self->pressIndex] = press;
  self->pressIndex = (self->pressIndex + 1) % 3;
  unsigned char valid = 1;
  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 3; j++) {
      if (abs(self->presses[i] - self->presses[j]) > 100) {
        valid = 0;
        break;
      }
    }
    print("%d ", self->presses[i]);
  }
  if (valid) {
    int tempo = calculateTempo(self->presses);
    print("Set tempo to %d BPM\n", tempo);
    ASYNC(&musicPlayer, setTempo, tempo);
  }
  print("\n");
  return 0;
}

int calculateTempo(int16_t presses[4]) {
  int sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += presses[i];
  }
  int average = sum / 4;
  int tempo = 60000 / average; // Convert ms per beat to BPM
  return tempo;
}
