#pragma once

#include "Arduino.h"
#include "common\common.h"
#include "common\wifi.h"
#include "components\hookSwitch.h"
#include "components\modem.h"
#include "components\ringer.h"
#include "components\rotaryDial.h"

class PhoneApp {
public:
  PhoneApp();

  void setup();

  void loop();

private:
  void setState(const AppState newState, const StateResult &result = StateResult());

  void processState();

  void processStateCheckHardware();

  void processStateCheckLine();

  void processStateIdle();

  void processStateIncomingCall();

  void processStateDialing();

  void processStateInCall();

  void onSetStateCheckHardwareState();

  void onSetStateCheckLineState();

  void onSetStateIdleState();

  void onSetStateIncomingCallState(const StateResult &result);

  void onSetStateIncomingCallRingState();

  void onSetStateInCallState();

  Modem modem;

  Ringer ringer;

  HookSwitch hookSwitch;

  RotaryDial rotaryDial;

  Wifi wifi;

  AppState state = AppState::Startup;

  unsigned long stateTime = 0;

  bool firstTimeSystemReady = false;
};
