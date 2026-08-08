#include "App.h"
#include "MusicPlayer.h"
#include "TinyTimber.h"
#include "ToneGenerator.h"
#include "Watchdog.h"
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

// Helper function to handle conductor claim arbitration
// Makes sure the highest nodeId within a 100ms window
static void handleConductorMessage(App *self, int nodeId) {
  if (isClaimTimedOut(self, CLAIM_TIMEOUT_MSEC)) {
    T_RESET(&self->claimTimer);
    self->currentConductor = nodeId;
    if (self->conductor) {
      setConductorMode(self, 0);
    }
    return;
  }
  if (nodeId > self->currentConductor) {
    self->currentConductor = nodeId;
    if (self->conductor) {
      setConductorMode(self, 0);
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
  ASYNC(self, sendPing, NULL);
  return 0;
}

/* ==========================================================================
 * CAN Message Handling
 * ========================================================================== */

int handleCanMessage(App *self, int unused) {
  CANMsg msg;
  CAN_RECEIVE(&can0, &msg);

  // Emulate silent failure by not processing any CAN messages if failed.
  if (self->failed)
    return 0;

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
      ASYNC(&watchdog, stopWatchdog, 0);
      ASYNC(self, stopPulse, 0);
      break;
    case 3: // Key command
      ASYNC(&musicPlayer, setKeyOffset, (int)(int8_t)msg.buff[1]);
      break;
    case 4: // Tempo command
      ASYNC(&musicPlayer, setTempoBpm, (msg.buff[1] << 8) | msg.buff[2]);
      break;
    }
    break;

  case MSG_PING:
    print("Ping received from node %d\n", msg.nodeId);
    registerNode(self, msg.nodeId);
    ASYNC(self, sendReply, msg.nodeId);
    break;

  case MSG_REPLY:
    print("Reply received from node %d\n", msg.nodeId);
    registerNode(self, msg.nodeId);
    if (self->aloneTask) {
      ABORT(self->aloneTask);
      self->aloneTask = NULL;
    }
    break;

  case MSG_CONDUCTOR:
    print("Conductor claim received from node %d\n", msg.nodeId);
    handleConductorMessage(self, msg.nodeId);
    break;

  case MSG_NOTE:
    SYNC(&musicPlayer, startPlayback, NULL);
    ASYNC(self, handleNote, msg.buff[0]);
    break;

  case MSG_NODE_FAILURE:
    print("Node %d failed\n", msg.buff[0]);
    if (msg.buff[0]) {
      if (msg.buff[0] == NODE_ID) {
        ASYNC(self, sendPing, NULL);
      } else {
        ASYNC(self, deleteNode, msg.buff[0]);
      }
    }
    break;

  case MSG_HEARTBEAT:
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
  int tmp;
  uint8_t volume;

  switch (c) {
  // Toggle failure mode type
  case 'f':
    self->failureMode ^= 1;
    print("Failure mode type: %s\n", self->failureMode ? "F2" : "F1");
    break;
  // Discover nodes
  case 'd':
    sendPing(self, 0);
    ASYNC(self, clearInputBuffer, 0);
    break;
  // Toggle mute
  case 'm':
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, toggleLedMute, NULL);
    break;
  // Increase volume
  case 'i':
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume + 1);
    break;
  // Decrease volume
  case 'u':
    ASYNC(self, clearInputBuffer, 0);
    volume = SYNC(&toneGenerator, getCurrentVolume, NULL);
    ASYNC(&toneGenerator, setOutputVolume, volume - 1);
    break;
  // Set key
  case 'k':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetKeyCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setKeyOffset, tmp);
    break;
  // Set tempo
  case 't':
    tmp = parseBufferAsInt(self);
    ASYNC(self, sendSetTempoCommand, tmp);
    ASYNC(self, clearInputBuffer, 0);
    if (self->conductor)
      ASYNC(&musicPlayer, setTempoBpm, tmp);
    break;
  // Play/stop toggle
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
  // Print network state
  case 'M':
    print("Current conductor: %d\n", self->currentConductor);
    print("Is conductor: %d\n", self->conductor);
    for (int i = 0; i < NODES_SIZE; i++) {
      if (self->nodes[i] != 0) {
        print("Node %d: %d\n", i, self->nodes[i]);
      }
    }
    ASYNC(self, clearInputBuffer, 0);
    break;
  // Reset tempo and key
  case 'R':
    ASYNC(self, setTempoBpm, 120);
    ASYNC(self, setKeyOffset, 0);
    ASYNC(self, sendResetCommand, 0);
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

// Sends a message to set status to playing.
int sendStartCommand(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 1;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a message to set status to stopped.
int sendStopCommand(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 2;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a message to set key
int sendSetKeyCommand(App *self, int key) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 2;
  msg.buff[0] = 3;
  msg.buff[1] = key;
  safeCanSend(self, &msg);
  return 0;
}

// Tempo is a 16 bit value, so we need to split it into two bytes.
int sendSetTempoCommand(App *self, int tempo) {
  CANMsg msg;
  msg.msgId = MSG_COMMAND;
  msg.nodeId = NODE_ID;
  msg.length = 3;
  msg.buff[0] = 4;
  msg.buff[1] = tempo >> 8 & 0xFF;
  msg.buff[2] = tempo & 0xFF;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a message to reset key and tempo.
int sendResetCommand(App *self, int unused) {
  ASYNC(self, sendSetTempoCommand, 120);
  ASYNC(self, sendSetKeyCommand, 0);
  return 0;
}

// Sends a discovery ping to find other nodes.
int sendPing(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_PING;
  msg.nodeId = NODE_ID;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a reply to a discovery ping. If we are conductor, also sends conductor
// claim along with current state.
int sendReply(App *self, int senderId) {
  CANMsg msg;
  msg.msgId = MSG_REPLY;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 1;
  safeCanSend(self, &msg);
  if (self->conductor) {
    ASYNC(self, sendConductorClaim, NULL);
    if (SYNC(&musicPlayer, getPlayingState, NULL)) {
      ASYNC(self, sendStartCommand, NULL);
    } else {
      ASYNC(self, sendStopCommand, NULL);
    }
    ASYNC(self, sendSetKeyCommand, SYNC(&musicPlayer, getKey, NULL));
    ASYNC(self, sendSetTempoCommand, SYNC(&musicPlayer, getTempo, NULL));
  }
  return 0;
}

// Sends a heartbeat message to notify other nodes that we are alive.
int sendHeartbeat(App *self, int unused) {
  CANMsg msg;
  msg.msgId = MSG_HEARTBEAT;
  msg.nodeId = NODE_ID;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a conductor claim message.
int sendConductorClaim(App *self, int unused) {
  if (!isClaimTimedOut(self, CLAIM_TIMEOUT_MSEC)) {
    if (NODE_ID > self->currentConductor) {
      self->currentConductor = NODE_ID;
      setConductorMode(self, 1);
    }
    return 0;
  }

  CANMsg msg;
  msg.msgId = MSG_CONDUCTOR;
  msg.nodeId = NODE_ID;
  safeCanSend(self, &msg);

  T_RESET(&self->claimTimer);
  self->currentConductor = NODE_ID;
  setConductorMode(self, 1);
  return 0;
}

// Sends a note for others to play.
int sendNote(App *self, int noteIdx) {
  if (self->failed || !SYNC(&musicPlayer, getPlayingState, NULL))
    return 0;
  handleNote(self, noteIdx);
  CANMsg msg;
  msg.buff[0] = noteIdx;
  msg.length = 1;
  msg.msgId = MSG_NOTE;
  msg.nodeId = NODE_ID;
  safeCanSend(self, &msg);
  return 0;
}

// Sends a message claiming that a node has failed.
int sendNodeFailure(App *self, int nodeId) {
  CANMsg msg;
  msg.buff[0] = nodeId;
  msg.length = 1;
  msg.msgId = MSG_NODE_FAILURE;
  msg.nodeId = NODE_ID;
  safeCanSend(self, &msg);
  return 0;
}

int handleNote(App *self, int note) {
  /* This makes sure all the surrounding things are handled when a note is
  supposed to be played. If the node is failed, we abort, we always stop the
  pulse if there is one from a previous node to prevent ghosts of our past
  haunting us. We schedule led toggling if conductor and either stop the
  watchdog if we play, or reset it if other node should play. */
  print("Handling note %d\n", note);
  stopPulse(self, 0);
  if (self->failed)
    return 0;
  if (self->conductor)
    ASYNC(self, scheduleLedToggle, note);
  if (shouldPlayNote(self, note)) {
    ASYNC(&watchdog, stopWatchdog, NULL);
    BEFORE(USEC(50), &musicPlayer, playNote, note);
    self->sendingHeartbeats = 1;
    self->pulseTask = ASYNC(self, pulse, NULL);
    return 0;
  }
  ASYNC(&watchdog, resetWatchdog, note);
  return 0;
}

int safeCanSend(App *self, CANMsg *msg) {
  if (CAN_SEND(&can0, msg)) {
    // Failed to send message. Enter recovery mode if not conductor.
    // If conductor keep going.
    // According to Problem 3 we should just keep playing if conductor in a 2
    // board setup.
    if (self->conductor && getRegisteredNodeCount(self, 0) < 3)
      return 1;
    ASYNC(self, enterRecoveryMode, NULL);
  }
  if (self->failed) {
    ASYNC(&musicPlayer, toggleLedMute, NULL);
    ASYNC(&musicPlayer, toggleLedMute, NULL);
  }
  self->failed = 0;
  return 1;
}

/* ==========================================================================
 * Conductor Mode Management
 * ==========================================================================
 */

// Sets local state conductor mode and updates LED to be correct.
void setConductorMode(App *self, int conductor) {
  self->conductor = conductor;
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

// Helper to check if > CLAIM_TIMEOUT_MSEC ms has passed since last claim
// message received/sent.
int isClaimTimedOut(App *self, int ms) {
  Time timeSinceLast = T_SAMPLE(&self->claimTimer);
  return timeSinceLast > MSEC(ms);
}

// Getter for current conductor to be used in the watchdog.
int getCurrentConductor(App *self, int unused) {
  return self->currentConductor;
}

/* ==========================================================================
 * Input Buffer Management
 * ==========================================================================
 */

// Clears the input buffer by resetting the index to 0.
int clearInputBuffer(App *self, int unused) {
  self->index = 0;
  return 0;
}

// Appends a character to the input buffer. If buffer is full, does nothing.
int appendToBuffer(App *self, int c) {
  if (self->index >= INPUT_BUFFER_SIZE) {
    self->index = INPUT_BUFFER_SIZE;
    return 0;
  }
  self->buf[self->index++] = c;
  return 0;
}

// Parses the input buffer as an integer and returns it.
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

// Attempts to registera nodeId in the set of known nodes. If the nodeId is
// already registered does nothing.
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

// Attempts to delete a node from the registered nodes. If the node is not
// found, does nothing.
int deleteNode(App *self, int nodeId) {
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] == nodeId) {
      self->nodes[i] = 0;
      return 0;
    }
  }
  print("Attempted to delete non-existent node %d\n", nodeId);
  return 0;
}

// Returns the order of a given nodeId among the registered nodes, starting from
// 0. E.g. if nodes 2, 5 and 9 are registered, getNodeOrder(5) will return 1,
// since node 5 is the second lowest nodeId among the registered nodes.
// getNodeOrder(2) would return 0 and getNodeOrder(9) would return 2.
// If the nodeId is not registered, returns -1.
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

// Returns the number of nodes known to the current node including itself.
int getRegisteredNodeCount(App *self, int unused) {
  int count = 0;
  for (int i = 0; i < NODES_SIZE; i++) {
    if (self->nodes[i] != 0) {
      count++;
    }
  }
  return count;
}

// Determines wether we should play a given note based on our rank and the
// amount of nodes.
int shouldPlayNote(App *self, int note) {
  int order = getNodeOrder(self, NODE_ID);
  int nodes = getRegisteredNodeCount(self, NODE_ID);
  return order == note % nodes;
}

// Returns the nodeId of the node that is expected to play a given note.
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

// Toggles led on/off. 1 is off, 0 is on.
int toggleLed(App *self, int UNUSED) {
  SIO_TOGGLE(&sio0);
  return 0;
}

// Toggles led to mute state.
int toggleLedMute(App *self, int UNUSED) {
  if (self->conductor)
    return 0;
  int muted = SYNC(&toneGenerator, toggleMuteState, NULL);
  SIO_WRITE(&sio0, muted);
  return 0;
}

// Schedules appropriate LED toggles for a given note.
int scheduleLedToggle(App *self, int melodyIndex) {
  // noteDuration is in microseconds
  int noteDuration = SYNC(&musicPlayer, getNoteDuration, melodyIndex);
  int rawDuration = SYNC(&musicPlayer, getRawDuration, melodyIndex);
  if (!self->conductor)
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

// Schedules a pulse task every 100ms to send heartbeats and notify the
// watchdog.
int pulse(App *self, int UNUSED) {
  if (!self->sendingHeartbeats || self->failed)
    return 0;
  self->pulseTask = AFTER(MSEC(100), self, pulse, NULL);
  ASYNC(&watchdog, notifyWatchdog, NODE_ID);
  ASYNC(self, sendHeartbeat, NULL);
  return 0;
}

// Stops the pulse tasks
int stopPulse(App *self, int UNUSED) {
  if (!self->pulseTask)
    return 0;
  ABORT(self->pulseTask);
  self->pulseTask = NULL;
  self->sendingHeartbeats = 0;
  return 0;
}

/* ==========================================================================
 * Failure Handling
 * ==========================================================================
 */

// Toggles failure mode on and off. In failure mode, the node stops responding
// to and sending messages.
int failureMode(App *self, int UNUSED) {
  self->failed ^= 1;
  SIO_WRITE(&sio0, self->failed);
  print("Failure mode: %s\n", self->failed ? "ON" : "OFF");
  if (self->failed) {
    ASYNC(&watchdog, stopWatchdog, NULL);
    if (self->failureMode == 1)
      AFTER(SEC(7), self, failureMode, 0);
  } else {
    ASYNC(self, sendPing, NULL);
    // Schedule to claim conductor and start melody if more than 500ms pass.
    // Then abort that task if receive conductor claim.
    self->aloneTask = AFTER(MSEC(500), self, amiAlone, NULL);
  }
  return 0;
}

// Runs if a ping is sent without getting a reply, starts playback
int amiAlone(App *self, int UNUSED) {
  self->aloneTask = NULL;
  print("I am alone. Claiming conductor and starting playback.\n");
  sendConductorClaim(self, 0);
  sendStartCommand(self, 0);
  SYNC(&musicPlayer, setTempoBpm, 120);
  SYNC(&musicPlayer, setKeyOffset, 0);
  sendResetCommand(self, 0);
  SYNC(&musicPlayer, startPlayback, 0);
  ASYNC(self, sendNote, 0);
  return 1;
}

int enterRecoveryMode(App *self, int UNUSED) {
  /* When in recovery mode, we don't want to send any can messages except for
  discovery pings. We set failed to 1 to prevent responding to discovery pings
  to ensure that we can recover using discovery ping -> exitRecoveryMode */
  setConductorMode(self, 0);
  self->currentConductor = 0;
  ASYNC(&musicPlayer, stopPlayback, NULL);
  ASYNC(&watchdog, stopWatchdog, NULL);
  stopPulse(self, 0);
  self->failed = 1;
  // 1 means light off.
  SIO_WRITE(&sio0, 1);
  print("Entering recovery mode\n");
  // Start sending discovery pings to find other nodes and recover
  // Since this enters safeCanSend it will recurse.
  AFTER(MSEC(100), self, sendPing, NULL);
  return 0;
}
