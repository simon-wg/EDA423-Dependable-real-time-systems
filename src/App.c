#include "App.h"
#include "MusicPlayer.h"
#include "TinyTimber.h"
#include "ToneGenerator.h"
#include "canTinyTimber.h"
#include "print.h"
#include "sciTinyTimber.h"
#include "sioTinyTimber.h"
#include <assert.h>
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
  switch (msg.msgId) {
  case MSG_COMMAND:
    print("Rcv(CAN): '%.*s'\n", msg.length, msg.buff);
    if (self->conductor) {
      return 0;
    }
    self->index = msg.length - 1;
    memcpy(self->buf, msg.buff, self->index);
    ASYNC(self, handleCan, msg.buff[msg.length - 1]);
    break;
  case MSG_HEARTBEAT:
    ASYNC(self, canHeartbeatResponse, 0);
    break;
  case MSG_RESPONSE:
    print("Heartbeat response received from node %d\n", msg.nodeId);
    print("Known nodes: ");
    addNode(self, msg.nodeId);
    for (int i = 0; i < NODES_SIZE; i++) {
      if (self->nodes[i] != 0) {
        print("%d ", self->nodes[i]);
      }
    }
    print("\n");
    break;
  default:
    print("Unknown CAN message received: id=%d, node=%d, length=%d\n",
          msg.msgId, msg.nodeId, msg.length);
    break;
  }
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
    ASYNC(self, clearBuffer, 0);
    break;
  case 'i':
    volume = SYNC(&toneGenerator, getVolume, NULL);
    ASYNC(&toneGenerator, setVolume, volume + 1);
    ASYNC(self, clearBuffer, 0);
    break;
  case 'u':
    volume = SYNC(&toneGenerator, getVolume, NULL);
    ASYNC(&toneGenerator, setVolume, volume - 1);
    ASYNC(self, clearBuffer, 0);
    break;
  case 't':
    n = getInt(self);
    ASYNC(&musicPlayer, setTempo, n);
    ASYNC(self, clearBuffer, 0);
    break;
  case 'k':
    n = getInt(self);
    ASYNC(&musicPlayer, setKey, n);
    ASYNC(self, clearBuffer, 0);
    break;
  case 'p':
    ASYNC(&musicPlayer, togglePlay, NULL);
    ASYNC(self, clearBuffer, 0);
    break;
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
    break;
  case 'd':
    canHeartbeat(self, 0);
    ASYNC(self, clearBuffer, 0);
    break;
  case 'm':
    canCommand(self, c);
    ASYNC(self, clearBuffer, 0);
    if (self->conductor)
      ASYNC(&toneGenerator, toggleMute, NULL);
    break;
  case 'i':
    canCommand(self, c);
    ASYNC(self, clearBuffer, 0);
    volume = SYNC(&toneGenerator, getVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setVolume, volume + 1);
    break;
  case 'u':
    canCommand(self, c);
    ASYNC(self, clearBuffer, 0);
    volume = SYNC(&toneGenerator, getVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setVolume, volume - 1);
    break;
  case 't':
    canCommand(self, c);
    n = getInt(self);
    ASYNC(self, clearBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempo, n);
    break;
  case 'k':
    canCommand(self, c);
    n = getInt(self);
    ASYNC(self, clearBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setKey, n);
    break;
  case 'p':
    canCommand(self, c);
    ASYNC(self, clearBuffer, 0);
    if (self->conductor) {
      ASYNC(&musicPlayer, togglePlay, NULL);
      ASYNC(&musicPlayer, toggleLogTempo, NULL);
    }
    break;
  default:
    ASYNC(self, appendBuffer, c);
    break;
  }
  return 0;
}

int canCommand(App *self, int c) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID; // Group id as node id for simplicity
  msg.length = self->index + 1;
  memcpy(msg.buff, self->buf, self->index);
  msg.buff[msg.length - 1] = c;
  CAN_SEND(&can0, &msg);
  return 0;
}

int canHeartbeat(App *self, int unused) {
  for (int i = 0; i < NODES_SIZE; i++)
    self->nodes[i] = 0;
  self->nodes[0] = NODE_ID;
  CANMsg msg;
  msg.msgId = MSG_HEARTBEAT;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int addNode(App *self, int nodeId) {
  // Check if nodeId already exists in the list
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == nodeId) {
      return 0;
    }
    if (self->nodes[i] == 0) { // Empty slot found, add nodeId here
      self->nodes[i] = nodeId;
      return 0;
    }
  }
}

int canHeartbeatResponse(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_RESPONSE;
  msg.nodeId = 2;
  // msg.nodeId = NODE_ID;
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

int compare(const void *a, const void *b) {
  unsigned char *valA = a;
  unsigned char *valB = b;
  return *valA - *valB;
}

int getOrder(App *self, int nodeId) {
  int size = sizeof(self->nodes) / sizeof(self->nodes[0]);
  qsort(self->nodes, size, sizeof(self->nodes[0]), compare);
  int leadingZeros = 0;
  for (int i = 0; i < size; i++) {
    if (self->nodes[i] == 0) {
      leadingZeros++;
    } else if (self->nodes[i] == nodeId) {
      return i - leadingZeros;
    }
  }
  return -1;
}

int getNodesCount(App *self, int unused) {
  int count = 0;
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] != 0) {
      count++;
    }
  }
  return count;
}
