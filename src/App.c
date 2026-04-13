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

/* External Objects */
extern App app;
extern ToneGenerator toneGenerator;
extern MusicPlayer musicPlayer;
extern Can can0;
extern Serial sci0;
extern SysIO sio0;

/* Private Helper Functions */
static int compare(const void *a, const void *b) {
  const unsigned char *valA = a;
  const unsigned char *valB = b;
  return *valA - *valB;
}

static void handleConductorMessage(App *self, int nodeId) {
  if (isClaimTimedOut(self, CLAIM_TIMEOUT_SEC)) {
    T_RESET(&self->claimTimer);
    self->currentConductor = nodeId;
    if (self->conductor) {
      toggleConductorMode(self);
    }
    return;
  }
  if (nodeId > self->currentConductor) {
    self->currentConductor = nodeId;
    if (self->conductor) {
      toggleConductorMode(self);
    }
  }
}

/* ==========================================================================
 * Initialization
 * ========================================================================== */

int main(void) {
  INSTALL(&can0, can_interrupt, CAN_IRQ0);
  INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
  INSTALL(&sio0, sio_interrupt, SIO_IRQ0);
  TINYTIMBER(&app, initialize, NULL);
  return 0;
}

int initialize(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SIO_INIT(&sio0);
  print("Hello world!\n");
  return 0;
}

/* ==========================================================================
 * CAN Message Handling
 * ========================================================================== */

int handleCanMessage(App *self, int unused) {
  CANMsg msg;
  CAN_RECEIVE(&can0, &msg);

  switch (msg.msgId) {
  case MSG_COMMAND:
    if (self->conductor) {
      return 0;
    }
    switch (msg.buff[0]) {
    case 1: // Start melody command
      print("Starting playback\n");
      ASYNC(&musicPlayer, startPlayback, 0);
      break;
    case 2: // Stop melody command
      print("Stopping playback\n");
      ASYNC(&musicPlayer, stopPlayback, 0);
      break;
    case 3: // Tempo command
      print("Setting tempo to %d BPM\n", msg.buff[1]);
      ASYNC(&musicPlayer, setTempoBpm, msg.buff[1]);
      break;
    case 4: // Key command
      print("Setting key offset to %d\n", msg.buff[1]);
      ASYNC(&musicPlayer, setKeyOffset, msg.buff[1]);
      break;
    }
    break;

  case MSG_HEARTBEAT:
    ASYNC(self, sendHeartbeatReply, 0);
    break;

  case MSG_RESPONSE:
    print("Heartbeat response received from node %d\n", msg.nodeId);
    print("Known nodes: ");
    registerNode(self, msg.nodeId);
    for (int i = 0; i < NODES_SIZE; i++) {
      if (self->nodes[i] != 0) {
        print("%d ", self->nodes[i]);
      }
    }
    print("\n");
    break;

  case MSG_CONDUCTOR:
    print("Conductor claim received from node %d\n", msg.nodeId);
    handleConductorMessage(self, msg.nodeId);
    break;

  case MSG_NOTE:
    ASYNC(&musicPlayer, playNote, msg.buff[0]);
    break;

  default:
    print("Unknown CAN message received: id=%d, node=%d, length=%d\n",
          msg.msgId, msg.nodeId, msg.length);
    break;
  }
  return 0;
}

/* ==========================================================================
 * Serial Input Handling
 * ========================================================================== */

int handleSerialInput(App *self, int c) {
  ASYNC(self, processSerialCommand, c);
  return 0;
}

int processSerialCommand(App *self, int c) {
  print("Rcv(SCI): '%c'\n", c);

  if (c == 'c') {
    toggleConductorMode(self);
    return 0;
  }

  int tmp;
  uint8_t volume;

  switch (c) {
  case 'F':
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'd':
    sendHeartbeat(self, 0);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'm':
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, toggleLedMute, NULL);
    break;
  case 'i':
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume + 1);
    break;
  case 'u':
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume - 1);
    break;
  case 't':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetTempoCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempoBpm, tmp);
    break;
  case 'k':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetKeyCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setKeyOffset, tmp);
    break;
  case 'p':
    tmp = SYNC(&musicPlayer, getPlayingState, NULL);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor) {
      switch (tmp) {
      case 0:
        SYNC(&musicPlayer, startPlayback, 0);
        ASYNC(self, sendStartCommand, 0);
        ASYNC(self, sendNote, 0);
        break;
      case 1:
        SYNC(&musicPlayer, stopPlayback, 0);
        ASYNC(self, sendStopCommand, 0);
        break;
      }
    }
    break;
  default:
    ASYNC(self, appendToBuffer, c);
    break;
  }
  return 0;
}

/* ==========================================================================
 * CAN Network Communication
 * ========================================================================== */

int sendStartCommand(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 1;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendStopCommand(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 2;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendSetKeyCommand(App *self, int key) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 2;
  msg.buff[0] = 4;
  msg.buff[1] = key;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendSetTempoCommand(App *self, int tempo) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 2;
  msg.buff[0] = 3;
  msg.buff[1] = tempo;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendResetCommand(App *self, int unused) {
  CANMsg tempoMsg;
  CANMsg keyMsg;

  tempoMsg.buff[0] = 4;
  tempoMsg.buff[1] = 120;
  keyMsg.buff[0] = 3;
  keyMsg.buff[1] = 0;
  tempoMsg.msgId = MSG_COMMAND;
  keyMsg.msgId = MSG_COMMAND;
  tempoMsg.nodeId = NODE_ID;
  keyMsg.nodeId = NODE_ID;
  tempoMsg.length = 2;
  keyMsg.length = 2;

  CAN_SEND(&can0, &tempoMsg);
  CAN_SEND(&can0, &keyMsg);
  return 0;
}

int sendHeartbeat(App *self, int unused) {
  for (int i = 0; i < NODES_SIZE; i++)
    self->nodes[i] = 0;
  self->nodes[0] = NODE_ID;

  CANMsg msg;
  msg.msgId = MSG_HEARTBEAT;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendHeartbeatReply(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_RESPONSE;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendConductorClaim(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_CONDUCTOR;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);

  if (!isClaimTimedOut(self, CLAIM_TIMEOUT_SEC)) {
    if (NODE_ID > self->currentConductor) {
      self->currentConductor = NODE_ID;
      toggleConductorMode(self);
    }
    return 0;
  }

  T_RESET(&self->claimTimer);
  self->currentConductor = NODE_ID;
  toggleConductorMode(self);
  return 0;
}

int sendNote(App *self, int note) {
  CANMsg msg;
  msg.buff[0] = note;
  msg.length = 1;
  msg.msgId = MSG_NOTE;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

/* ==========================================================================
 * Conductor Mode Management
 * ========================================================================== */

void toggleConductorMode(App *self) {
  self->conductor = !self->conductor;
  print("Conductor mode: %s\n", self->conductor ? "ON" : "OFF");

  if (!self->conductor) {
    // Set light to the actual muted state when losing conductor status
    ASYNC(&musicPlayer, toggleLedMute, NULL);
    ASYNC(&musicPlayer, toggleLedMute, NULL);
  } else {
    // Turn off light when becoming conductor
    SIO_WRITE(&sio0, 1);
  }
}

int hasConductorRole(App *self, int unused) { return self->conductor; }

int isClaimTimedOut(App *self, int ms) {
  Time timeSinceLast = T_SAMPLE(&self->claimTimer);
  return timeSinceLast > 2 * MSEC(ms);
}

/* ==========================================================================
 * Input Buffer Management
 * ========================================================================== */

int clearInputBuffer(App *self, int unused) {
  self->index = 0;
  return 0;
}

int appendToBuffer(App *self, int c) {
  if (self->index >= INPUT_BUFFER_SIZE) {
    self->index = INPUT_BUFFER_SIZE;
    return 0;
  }
  self->buf[self->index++] = c;
  return 0;
}

int parseBufferAsInt(App *self) {
  char strBuf[INPUT_BUFFER_SIZE + 1];
  for (int i = 0; i < self->index; i++) {
    strBuf[i] = (char)self->buf[i];
  }
  strBuf[self->index] = '\0';
  self->index = 0;
  return atoi(strBuf);
}

/* ==========================================================================
 * Node Discovery and Management
 * ========================================================================== */

int registerNode(App *self, int nodeId) {
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == nodeId) {
      return 0;
    }
    if (self->nodes[i] == 0) {
      self->nodes[i] = nodeId;
      return 0;
    }
  }
  return 0;
}

int getNodeOrder(App *self, int nodeId) {
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

int getRegisteredNodeCount(App *self, int unused) {
  int count = 0;
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] != 0) {
      count++;
    }
  }
  return count;
}
