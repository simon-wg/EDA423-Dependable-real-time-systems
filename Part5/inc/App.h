#ifndef _APP_H
#define _APP_H

#include "TinyTimber.h"
#include "canTinyTimber.h"
#include <stdint.h>

#define RECIEVER_BUFFER_SIZE 10
#define MAX_SEQ_NO 128
#define NODE_ID 1

/*
 * App Class Definition
 */

typedef struct {
  Object super;
  Timer lastDeliveryTime;
  Time delta;
  uint8_t recieverBuff[RECIEVER_BUFFER_SIZE];
  unsigned char receiverHead;
  unsigned char receiverTail;
  char ready;
  unsigned char debug;
  unsigned char canSeqNo;
  Msg burstTask;
} App;

#define initApp() {initObject(), initTimer(), SEC(1), {NULL}, 0, 0, 1, 0, 0, NULL}

/*
 * App Class Methods
 */

int handleSerialInput(App *, int);
int receiver(App *, int);
int handleCanMessage(App *, int);
void addMsgToBuffer(App *, char);
int initialize(App *, int);
int processSerialCommand(App *, int);
int processCanCommand(App *, int);
int sendCanMessage(App *, int);
int canBurst(App *, int);
int stopBurstTask(App *, int);

#endif
