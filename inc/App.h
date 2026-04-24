#ifndef _APP_H
#define _APP_H

#include "TinyTimber.h"
#include "canTinyTimber.h"

#define INPUT_BUFFER_SIZE 10
#define NODE_ID 1
#define NODES_SIZE 3
#define CLAIM_TIMEOUT_SEC 5

typedef enum {
  MSG_COMMAND = 1,
  MSG_PING = 2,
  MSG_REPLY = 3,
  MSG_CONDUCTOR = 4,
  MSG_NOTE = 5,
  MSG_HEARTBEAT = 6,
  MSG_NODE_FAILURE = 7,
} MessageType;

/*
 * App Class Definition
 */

typedef struct {
  Object super;
  Timer claimTimer;
  Timer heartBeatTimer;
  char buf[INPUT_BUFFER_SIZE];
  unsigned char index;
  unsigned char conductor;
  unsigned char currentConductor;
  unsigned char failed;
  unsigned char sendingHeartbeats;
  Msg pulseTask;
  unsigned char nodes[NODES_SIZE];
  unsigned char failureMode;
} App;

#define initApp() {initObject(), initTimer(), initTimer(), {0}, 0, 0, 0, 0, 0, NULL, {NODE_ID, 0, 0}, 0}

/*
 * App Class Methods
 */

int handleSerialInput(App *, int);
int handleCanMessage(App *, int);
int initialize(App *, int);
int processSerialCommand(App *, int);
int processCanCommand(App *, int);
int sendStartCommand(App *, int);
int sendStopCommand(App *, int);
int sendSetKeyCommand(App *, int);
int sendSetTempoCommand(App *, int);
int sendResetCommand(App *, int);
int sendPing(App *, int);
int sendReply(App *, int);
int sendHeartbeat(App *, int);
int sendNote(App *, int);
int sendNodeFailure(App *, int);
int sendConductorClaim(App *, int);
int getCurrentConductor(App *, int);
int handleNote(App *, int);
int registerNode(App *, int);
int deleteNode(App *, int);
int getExpectedNodeForNote(App *, int);
int clearInputBuffer(App *, int);
int appendToBuffer(App *, int);
int scheduleLedToggle(App *, int);
int toggleLed(App *, int);
int toggleLedMute(App *, int);
int pulse(App *, int);
int stopPulse(App *, int);
int failureMode(App *, int);
int enterRecoveryMode(App *, int);

/*
 * Helper Functions
 */

int safeCanSend(App *, CANMsg *);
int parseBufferAsInt(App *);
int getNodeOrder(App *, int);
int getRegisteredNodeCount(App *, int);
int shouldPlayNote(App *, int);
int hasConductorRole(App *, int);
int isClaimTimedOut(App *, int);
void setConductorMode(App *, int);

#endif
