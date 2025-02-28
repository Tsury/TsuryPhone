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

  Modem _modem;
  Ringer _ringer;
  HookSwitch _hookSwitch;
  RotaryDial _rotaryDial;
  Wifi _wifi;
  AppState _state = AppState::Startup;

  unsigned long _stateTime = 0;
  bool _firstTimeSystemReady = false;
};
