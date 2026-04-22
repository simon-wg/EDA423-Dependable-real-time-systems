#include "App.h"
#include "MusicPlayer.h"
#include "TinyTimber.h"
#include "ToneGenerator.h"
#include "Watchdog.h"
#include "canTinyTimber.h"
#include "print.h"
#include "sciTinyTimber.h"
#include "sioTinyTimber.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* External Objects */
extern App app;
extern Watchdog watchdog;
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
  ASYNC(self, registerNode, NODE_ID);
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
    if (self->conductor || self->failed) {
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
    case 3: // Key command
      ASYNC(&musicPlayer, setKeyOffset, msg.buff[1]);
      break;
    case 4: // Tempo command
      ASYNC(&musicPlayer, setTempoBpm, (msg.buff[1] << 8) | msg.buff[2]);
      break;
    }
    break;

  case MSG_PING:
    if (self->failed) {
      return 0;
    }
    print("Ping received from node %d\n", msg.nodeId);
    registerNode(self, msg.nodeId);
    ASYNC(self, sendReply, msg.nodeId);
    break;

  case MSG_REPLY:
    if (self->failed) {
      return 0;
    }
    print("Reply received from node %d\n", msg.nodeId);
    registerNode(self, msg.nodeId);
    break;

  case MSG_CONDUCTOR:
    if (self->failed) {
      return 0;
    }
    print("Conductor claim received from node %d\n", msg.nodeId);
    handleConductorMessage(self, msg.nodeId);
    break;

  case MSG_NOTE:
    if (self->failed) {
      return 0;
    }
    ASYNC(self, handleNote, msg.buff[0]);
    break;
  case MSG_HEARTBEAT:
    if (self->failed) {
      return 0;
    }
    print("Heartbeat received from node %d\n", msg.nodeId);
    ASYNC(&watchdog, notifyWatchdog, msg.nodeId);
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
 * ==========================================================================
 */

int handleSerialInput(App *self, int c) {
  print("Rcv(SCI): '%c'\n", c);
  ASYNC(self, processSerialCommand, c);
  return 0;
}

int processSerialCommand(App *self, int c) {
  if (c == 'c') {
    ASYNC(self, toggleConductorMode, 0);
    return 0;
  }

  int tmp;
  uint8_t volume;

  switch (c) {
  case 'F':
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'd':
    sendPing(self, 0);
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
  case 'k':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetKeyCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setKeyOffset, tmp);
    break;
  case 't':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetTempoCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempoBpm, tmp);
    break;
  case 'p':
    tmp = SYNC(&musicPlayer, getPlayingState, NULL);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor) {
      switch (tmp) {
      case 0:
        SYNC(&musicPlayer, startPlayback, 0);
        sendStartCommand(self, 0);
        AFTER(MSEC(1), &app, sendNote, 0);
        break;
      case 1:
        SYNC(&musicPlayer, stopPlayback, 0);
        ASYNC(self, sendStopCommand, 0);
        SYNC(&watchdog, stopWatchdog, 0);
        break;
      }
    }
    break;
  case 'M':
    print("Current conductor: %d\n", self->currentConductor);
    for (int i = 0; i < NODES_SIZE; i++) {
      if (self->nodes[i] != 0) {
        print("Node %d: %d\n", i, self->nodes[i]);
      }
    }
    ASYNC(self, clearInputBuffer, 0);
    break;
  default:
    ASYNC(self, appendToBuffer, c);
    break;
  }
  return 0;
}

/* ==========================================================================
 * CAN Network Communication
 * ==========================================================================
 */

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
  msg.buff[0] = 3;
  msg.buff[1] = key;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendSetTempoCommand(App *self, int tempo) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 3;
  msg.buff[0] = 4;
  msg.buff[1] = tempo >> 8 & 0xFF;
  msg.buff[2] = tempo & 0xFF;
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

int sendPing(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_PING;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendReply(App *self, int senderId) {
  CANMsg msg;
  msg.msgId = MSG_REPLY;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 1;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendHeartbeat(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_HEARTBEAT;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int sendConductorClaim(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_CONDUCTOR;
  msg.nodeId = NODE_ID;
  msg.length = 0;
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
  if (handleNote(self, note))
    return 0;
  CANMsg msg;
  msg.buff[0] = note;
  msg.length = 1;
  msg.msgId = MSG_NOTE;
  msg.nodeId = NODE_ID;
  CAN_SEND(&can0, &msg);
  return 0;
}

int handleNote(App *self, int note) {
  stopPulse(self, 0);
  ASYNC(&watchdog, resetWatchdog, note);
  if (self->conductor) {
    ASYNC(self, scheduleLedToggle, note);
  }
  if (shouldPlayNote(self, note)) {
    print("Playing note %d\n", note);
    BEFORE(USEC(50), &musicPlayer, playNote, note);
    self->sendingHeartbeats = 1;
    self->pulseTask = ASYNC(self, pulse, NULL);
    return 1;
  }
  return 0;
}

/* ==========================================================================
 * Conductor Mode Management
 * ==========================================================================
 */

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

int getCurrentConductor(App *self, int unused) {
  return self->currentConductor;
}

/* ==========================================================================
 * Input Buffer Management
 * ==========================================================================
 */

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
 * ==========================================================================
 */

int registerNode(App *self, int nodeId) {
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == nodeId) {
      return 0;
    }
  }
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == 0) {
      self->nodes[i] = nodeId;
      return 0;
    }
  }
  return 0;
}

int deleteNode(App *self, int nodeId) {
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == nodeId) {
      self->nodes[i] = 0;
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

int shouldPlayNote(App *self, int nodeNote) {
  int order = getNodeOrder(self, NODE_ID);
  int nodes = getRegisteredNodeCount(self, NODE_ID);
  return order == nodeNote % nodes;
}

int getExpectedNodeForNote(App *self, int note) {
  int nodeCount = getRegisteredNodeCount(self, NODE_ID);
  if (nodeCount == 0) {
    return -1;
  }
  int expectedOrder = note % nodeCount;
  for (int i = 0; i < NODES_SIZE; i++) {
    if (getNodeOrder(self, self->nodes[i]) == expectedOrder) {
      return self->nodes[i];
    }
  }
  return -1;
}

/* ==========================================================================
 * LED Control
 * ==========================================================================
 */

int toggleLed(App *self, int UNUSED) {
  SIO_TOGGLE(&sio0);
  return 0;
}

int toggleLedMute(App *self, int UNUSED) {
  if (hasConductorRole(self, 0))
    return 0;
  int muted = SYNC(&toneGenerator, toggleMuteState, NULL);
  SIO_WRITE(&sio0, muted);
  return 0;
}

int scheduleLedToggle(App *self, int melodyIndex) {
  // noteDuration is in microseconds
  int noteDuration = SYNC(&musicPlayer, getNoteDuration, melodyIndex);
  int rawDuration = SYNC(&musicPlayer, getRawDuration, melodyIndex);
  if (!hasConductorRole(self, 0))
    return 0;

  int numToggles = 8 / rawDuration;
  int toggleInterval = noteDuration / numToggles;

  for (int i = 0; i < numToggles; i++) {
    AFTER(USEC(toggleInterval * i), self, toggleLed, NULL);
  }
  return 0;
}

/* ==========================================================================
 * Heartbeat Scheduling
 * ==========================================================================
 */

int pulse(App *self, int UNUSED) {
  if (!self->sendingHeartbeats)
    return 0;
  self->pulseTask = AFTER(MSEC(100), self, pulse, NULL);
  ASYNC(&watchdog, notifyWatchdog, NODE_ID);
  ASYNC(self, sendHeartbeat, NULL);
  return 0;
}

int stopPulse(App *self, int UNUSED) {
  ABORT(self->pulseTask);
  self->sendingHeartbeats = 0;
  return 0;
}

/* ==========================================================================
 * Failure Handling
 * ==========================================================================
 */

int failureMode(App *self, int mode) {
  self->failed ^= 1;
  if (mode == 2) {
    AFTER(SEC(7), self, failureMode, 0);
  }
  return 0;
}
