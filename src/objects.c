#include "App.h"
#include "Button.h"
#include "MusicPlayer.h"
#include "ToneGenerator.h"
#include "canTinyTimber.h"
#include "sciTinyTimber.h"
#include "sioTinyTimber.h"

App app = initApp();
MusicPlayer musicPlayer = initMusicPlayer();
ToneGenerator toneGenerator = initToneGenerator(4);
Can can0 = initCan(CAN_PORT0, &app, receiver);
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Button button = initButton();
SysIO sio0 = initSysIO(SIO_PORT0, &button, buttonPress);
