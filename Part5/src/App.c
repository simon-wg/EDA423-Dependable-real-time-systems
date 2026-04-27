#include "App.h"
#include "TinyTimber.h"
#include "canTinyTimber.h"
#include "print.h"
#include "sciTinyTimber.h"
#include <assert.h>
#include <string.h>

/* External Objects */
extern App app;
extern Can can0;
extern Serial sci0;

/* ==========================================================================
 * Initialization
 * ========================================================================== */

int main(void) {
  INSTALL(&can0, can_interrupt, CAN_IRQ0);
  INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
  TINYTIMBER(&app, initialize, NULL);
  return 0;
}

int initialize(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  print("Hello world!\n");
  return 0;
}

/* ==========================================================================
 * CAN Message Handling
 * ========================================================================== */

// Old name was receiver
int receiver(App *self, int unused) {
  CANMsg msg;
  // We want to regulate the arrival time of CAN messages.
  CAN_RECEIVE(&can0, &msg);
  addMsgToBuffer(self, msg.msgId);
  if (self->ready) {
    handleCanMessage(self, 0);
    self->ready = 0;
  }
  return 0;
}

int handleCanMessage(App *self, int unused) {
  if (self->receiverHead == self->receiverTail) {
    self->ready = 1;
    return 0;
  }
  print("Handling msg with ID %d\n", self->recieverBuff[self->receiverHead]);
  self->receiverHead = (self->receiverHead + 1) % RECIEVER_BUFFER_SIZE;
  AFTER(self->delta, self, handleCanMessage, 0);
  return 0;
}

void addMsgToBuffer(App *self, char msgId) {
  if (self->receiverTail + 1 % RECIEVER_BUFFER_SIZE == self->receiverHead) {
    print("Buffer overflow! Message with ID %d was dropped.\n", msgId);
    self->canSeqNo--;
    return;
  }

  self->recieverBuff[self->receiverTail] = msgId;
  self->receiverTail = (self->receiverTail + 1) % RECIEVER_BUFFER_SIZE;
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
  switch (c) {
  case 'O':
    sendCanMessage(self, 0);
    break;
  case 'B':
    canBurst(self, 0);
    break;
  case 'X':
    stopBurstTask(self, 0);
    break;
  // Change arrival time delta.
  case 'T':
    switch (self->delta) {
    case SEC(1):
      self->delta = SEC(2);
      print("Changing CAN message handling delta to 2 seconds.\n");
      break;
    case SEC(2):
      self->delta = SEC(5);
      print("Changing CAN message handling delta to 5 seconds.\n");
      break;
    case SEC(5):
      self->delta = SEC(1);
      print("Changing CAN message handling delta to 1 seconds.\n");
      break;
    }
  // Toggle debug printing.
  case 'D':
    self->debug ^= 1;
    break;
  default:
    break;
  }
  return 0;
}

/* ==========================================================================
 * CAN Network Communication
 * ==========================================================================
 */

int canBurst(App *self, int unused) {
  sendCanMessage(self, 0);
  self->burstTask = AFTER(MSEC(50), self, canBurst, 0);
  return 0;
}

int stopBurstTask(App *self, int unused) {
  if (self->burstTask) {
    ABORT(self->burstTask);
    self->burstTask = NULL;
  }
  return 0;
}

// Sends a message to set status to playing.
int sendCanMessage(App *self, int unused) {
  CANMsg msg;
  msg.msgId = self->canSeqNo;
  debug(self->debug, "Sending msg with ID %d\n", self->canSeqNo);
  self->canSeqNo = (self->canSeqNo + 1) % MAX_SEQ_NO;
  msg.nodeId = NODE_ID;
  msg.length = 1;
  msg.buff[0] = 1;
  CAN_SEND(&can0, &msg);
  return 0;
}
