#pragma once

#include "common\consts.h"
#include "common\timeManager.h"
#include "common\wifi.h"
#include "components\hookSwitch.h"
#include "components\modem.h"
#include "components\ringer.h"
#include "components\rotaryDial.h"
#include "core\DeviceConfig.h"
#include "core\DeviceStats.h"
#include "core\NumberHandler.h"
#include "integration\IntegrationManager.h"
#include <Arduino.h>

class PhoneApp {
public:
  PhoneApp();

  void setup();
  void loop();

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

  // Audio configuration change handler
  void onAudioConfigChanged();
  
  // Maintenance mode change handler
  void onMaintenanceModeChanged();

  // Integration operation callbacks
  bool handleIntegrationDialRequest(const String& number);
  bool handleIntegrationAnswerRequest();
  bool handleIntegrationHangupRequest();
  bool handleIntegrationRingRequest(const String& pattern);
  bool handleIntegrationWebhookTrigger(const String& webhookId);
  bool handleIntegrationCallWaitingRequest();

  DeviceConfig _deviceConfig;
  DeviceStats _deviceStats;
  Modem _modem;
  Ringer _ringer;
  HookSwitch _hookSwitch;
  RotaryDial _rotaryDial;
  Wifi _wifi;
  TimeManager _timeManager;
  NumberHandler _numberHandler;
  IntegrationManager _integrationManager;
  State _state = {AppState::Startup, AppState::Startup, CallState(), "", false, false};

  uint32_t _stateTime = 0UL;
  bool _firstTimeSystemReady = false;
};
