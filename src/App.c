#include "App.h"
#include "MusicPlayer.h"
#include "TinyTimber.h"
#include "ToneGenerator.h"
#include "canTinyTimber.h"
#include "print.h"
#include "sciTinyTimber.h"
#include "sioTinyTimber.h"
#include <stdlib.h>
#include <string.h>

extern App app;
extern ToneGenerator toneGenerator;
extern MusicPlayer musicPlayer;
extern Can can0;
extern Serial sci0;
extern SysIO sio0;

int receiver(App *self, int unused) {
  CANMsg msg;
  CAN_RECEIVE(&can0, &msg);
  print("Rcv(CAN): '%s'\n", msg.buff);
  if (self->conductor) {
    return 0;
  }
  self->index = msg.length - 1;
  memcpy(self->buf, msg.buff, self->index);
  ASYNC(self, handleCan, msg.buff[msg.length - 1]);
  return 0;
}

int reader(App *self, int c) {
  ASYNC(self, handleSerial, c);
  return 0;
}

int startApp(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SIO_INIT(&sio0);
  ASYNC(&musicPlayer, toggleLight, NULL);
  print("Hello world!\n");
  return 0;
}

int main() {
  INSTALL(&can0, can_interrupt, CAN_IRQ0);
  INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
  INSTALL(&sio0, sio_interrupt, SIO_IRQ0);
  TINYTIMBER(&app, startApp, NULL);
  return 0;
}

int handleCan(App *self, int c) {
  int n;
  uint8_t volume;
  switch (c) {
  case 'm':
    ASYNC(&toneGenerator, toggleMute, NULL);
    return 0;
  case 'i':
    volume = SYNC(&toneGenerator, getVolume, NULL);
    ASYNC(&toneGenerator, setVolume, volume + 1);
    return 0;
  case 'u':
    volume = SYNC(&toneGenerator, getVolume, NULL);
    ASYNC(&toneGenerator, setVolume, volume - 1);
    return 0;
  case 't':
    n = getInt(self);
    ASYNC(&musicPlayer, setTempo, n);
    return 0;
  case 'k':
    n = getInt(self);
    ASYNC(&musicPlayer, setKey, n);
    return 0;
  case 'p':
    ASYNC(&musicPlayer, togglePlay, NULL);
    return 0;
  }
  return 0;
}

int handleSerial(App *self, int c) {
  print("Rcv(SCI): '%c'\n", c);
  if (c == 'c') {
    self->conductor = !self->conductor;
    print("Conductor mode: %s\n", self->conductor ? "ON" : "OFF");
    return 0;
  }
  int n;
  uint8_t volume;
  switch (c) {
  case 'F':
    ASYNC(self, clearBuffer, 0);
    return 0;
  case 'm':
    sendCan(self, c);
    if (self->conductor)
      ASYNC(&toneGenerator, toggleMute, NULL);
    return 0;
  case 'i':
    sendCan(self, c);
    volume = SYNC(&toneGenerator, getVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setVolume, volume + 1);
    return 0;
  case 'u':
    sendCan(self, c);
    volume = SYNC(&toneGenerator, getVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setVolume, volume - 1);
    return 0;
  case 't':
    sendCan(self, c);
    n = getInt(self);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempo, n);
    return 0;
  case 'k':
    sendCan(self, c);
    n = getInt(self);
    if (self->conductor)
      ASYNC(&musicPlayer, setKey, n);
    return 0;
  case 'p':
    sendCan(self, c);
    if (self->conductor)
      ASYNC(&musicPlayer, togglePlay, NULL);
    return 0;
  default:
    ASYNC(self, appendBuffer, c);
    return 0;
  }
}

int sendCan(App *self, int c) {
  CANMsg msg;
  msg.msgId = 1;
  msg.nodeId = 1;
  msg.length = self->index + 1;
  memcpy(msg.buff, self->buf, self->index);
  msg.buff[msg.length - 1] = c;
  CAN_SEND(&can0, &msg);
  return 0;
}

int clearBuffer(App *self, int unused) {
  self->index = 0;
  return 0;
}

int appendBuffer(App *self, int c) {
  if (self->index >= INPUT_BUFFER_SIZE) {
    self->index = INPUT_BUFFER_SIZE;
    return 0;
  }
  self->buf[self->index++] = c;
  return 0;
}

int getInt(App *self) {
  char strBuf[INPUT_BUFFER_SIZE + 1];
  for (int i = 0; i < self->index; i++) {
    strBuf[i] = (char)self->buf[i];
  }
  strBuf[self->index] = '\0';
  self->index = 0;
  return atoi(strBuf);
}
