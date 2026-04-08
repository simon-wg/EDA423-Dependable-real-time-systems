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
  ASYNC(&musicPlayer, toggleLed, NULL);
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
    print("Rcv(CAN): '%.*s'\n", msg.length, msg.buff);
    if (self->conductor) {
      return 0;
    }
    self->index = msg.length - 1;
    memcpy(self->buf, msg.buff, self->index);
    ASYNC(self, processCanCommand, msg.buff[msg.length - 1]);
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
    handleConductorMessage(self, msg.nodeId);
    break;

  default:
    print("Unknown CAN message received: id=%d, node=%d, length=%d\n",
          msg.msgId, msg.nodeId, msg.length);
    break;
  }
  return 0;
}

int processCanCommand(App *self, int c) {
  int n;
  uint8_t volume;

  switch (c) {
  case 'm':
    ASYNC(&musicPlayer, toggleLedMute, NULL);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'i':
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume + 1);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'u':
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume - 1);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 't':
    n = parseBufferAsInt(self);
    ASYNC(&musicPlayer, setTempoBpm, n);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'k':
    n = parseBufferAsInt(self);
    ASYNC(&musicPlayer, setKeyOffset, n);
    ASYNC(self, clearInputBuffer, 0);
    break;
  case 'p':
    ASYNC(&musicPlayer, togglePlayback, NULL);
    ASYNC(self, clearInputBuffer, 0);
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

  int n;
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
    broadcastCanCommand(self, c);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, toggleLedMute, NULL);
    break;
  case 'i':
    broadcastCanCommand(self, c);
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setOutputVolume, volume + 1);
    break;
  case 'u':
    broadcastCanCommand(self, c);
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    if (self->conductor)
      ASYNC(&toneGenerator, setOutputVolume, volume - 1);
    break;
  case 't':
    broadcastCanCommand(self, c);
    n = parseBufferAsInt(self);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempoBpm, n);
    break;
  case 'k':
    broadcastCanCommand(self, c);
    n = parseBufferAsInt(self);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setKeyOffset, n);
    break;
  case 'p':
    broadcastCanCommand(self, c);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor) {
      SYNC(&musicPlayer, togglePlayback, NULL);
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

int broadcastCanCommand(App *self, int c) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = self->index + 1;
  memcpy(msg.buff, self->buf, self->index);
  msg.buff[msg.length - 1] = c;
  CAN_SEND(&can0, &msg);
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

int sendResetCommand(App *self, int unused) {
  CANMsg tempoMsg;
  CANMsg keyMsg;

  memcpy(tempoMsg.buff, "120t", 4);
  memcpy(keyMsg.buff, "0k", 3);
  tempoMsg.msgId = MSG_COMMAND;
  keyMsg.msgId = MSG_COMMAND;
  tempoMsg.nodeId = NODE_ID;
  keyMsg.nodeId = NODE_ID;
  tempoMsg.length = 4;
  keyMsg.length = 3;

  CAN_SEND(&can0, &tempoMsg);
  CAN_SEND(&can0, &keyMsg);
  return 0;
}

int sendHeartbeatReply(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_RESPONSE;
  msg.nodeId = 2;
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

int hasConductorRole(App *self, int unused) {
  return self->conductor;
}

int isClaimTimedOut(App *self, int ms) {
  Time timeSinceLast = T_SAMPLE(&self->claimTimer);
  return timeSinceLast > 100 * SEC(ms);
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
