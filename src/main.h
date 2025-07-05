#pragma once

#include "common\consts.h"
#include "common\timeManager.h"
#include "common\wifi.h"
#ifdef HOME_ASSISTANT_INTEGRATION
#include "common\homeAssistantServer.h"
#endif
#include "components\hookSwitch.h"
#include "components\modem.h"
#include "components\ringer.h"
#include "components\rotaryDial.h"
#include <Arduino.h>

// Forward declaration
class PhoneApp;
extern PhoneApp *g_phoneApp;

class PhoneApp {
public:
  PhoneApp();

  void setup();
  void loop();

#ifdef HOME_ASSISTANT_INTEGRATION
  // HA Integration callbacks
  void haPerformCall(const char *number);
  void haPerformHangup();
  void haPerformReset();
  void haPerformRing(int durationMs);
  void haSetDndEnabled(bool enabled);
  void haSetDndHours(int startHour, int startMinute, int endHour, int endMinute);
#endif

private:
  void setState(const AppState newState);

  void onStateChanged();

  void onStateCheckHardware();
  void onStateCheckLine();
  void onStateIdle();
  void onStateIncomingCall();
  void onStateInCall();

  void processState();

  void processStateCheckHardware();
  void processStateCheckLine();
  void processStateIdle();
  void processStateIncomingCall();
  void processStateDialing();
  void processStateInCall();
  void processStateInvalidNumber();

  void stopEverything();

  Modem _modem;
  Ringer _ringer;
  HookSwitch _hookSwitch;
  RotaryDial _rotaryDial;
  Wifi _wifi;
  TimeManager _timeManager;
  State _state{};

  uint32_t _stateTime = 0UL;
  bool _firstTimeSystemReady = false;
};
