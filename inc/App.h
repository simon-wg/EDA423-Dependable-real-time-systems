#ifndef _APP_H
#define _APP_H

#include "TinyTimber.h"

#define INPUT_BUFFER_SIZE 10
#define NODE_ID 1
#define NODES_SIZE 3

typedef enum {
  MSG_COMMAND = 1,
  MSG_HEARTBEAT = 2,
  MSG_RESPONSE = 3,
} MessageType;

typedef struct {
  Object super;
  char buf[INPUT_BUFFER_SIZE];
  unsigned char index;
  unsigned char conductor;
  unsigned char playing;
  unsigned char nodes[NODES_SIZE];
} App;

#define initApp() {initObject(), {0}, 0, 1, 0, {NODE_ID, 0, 0}}

int reader(App *, int);
int receiver(App *, int);
int startApp(App *, int);
int handleSerial(App *, int);
int handleCan(App *, int);
int canCommand(App *, int);
int canHeartbeat(App *, int);
int addNode(App *, int);
int canHeartbeatResponse(App *, int);
int clearBuffer(App *, int);
int appendBuffer(App *, int);

int getInt(App *);

int getOrder(App *, int);
int getNodesCount(App *, int);


#endif
