#ifndef _APP_H
#define _APP_H

#include "TinyTimber.h"

#define INPUT_BUFFER_SIZE 10
#define NODE_ID 1
#define NODES_SIZE 3
#define CLAIM_TIMEOUT_SEC 5

typedef enum {
  MSG_COMMAND = 1,
  MSG_HEARTBEAT = 2,
  MSG_RESPONSE = 3,
  MSG_CONDUCTOR = 4,
  MSG_NOTE = 5,
} MessageType;

/*
 * App Class Definition
 */

typedef struct {
  Object super;
  Timer claimTimer;
  char buf[INPUT_BUFFER_SIZE];
  unsigned char index;
  unsigned char conductor;
  unsigned char currentConductor;
  unsigned char playing;
  unsigned char nodes[NODES_SIZE];
} App;

#define initApp() {initObject(), initTimer(), {0}, 0, 0, 0, 0, {NODE_ID, 0, 0}}

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
int sendHeartbeat(App *, int);
int sendHeartbeatReply(App *, int);
int sendConductorClaim(App *, int);
int sendNote(App *, int);
int registerNode(App *, int);
int clearInputBuffer(App *, int);
int appendToBuffer(App *, int);

/*
 * Helper Functions
 */

int parseBufferAsInt(App *);
int getNodeOrder(App *, int);
int getRegisteredNodeCount(App *, int);
int hasConductorRole(App *, int);
int isClaimTimedOut(App *, int);
void toggleConductorMode(App *);

#endif
