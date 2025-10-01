#pragma once

#include "common\consts.h"
#include "common\timeManager.h"
#include "common\wifi.h"
#include "components\hookSwitch.h"
#include "components\modem.h"
#include "components\ringer.h"
#include "components\rotaryDial.h"
#include "core\DeviceConfig.h"
#include "core\NumberHandler.h"
#include <Arduino.h>
#include <memory>

#include "integration/IntegrationShim.h"

class TsuryPhone {
public:
  TsuryPhone();

  void setup();
  void loop();

  // Integration callbacks (no-op when integration disabled via stub)
  IntegrationCallbackResult handleIntegrationDialRequest(const String &number);
  IntegrationCallbackResult handleIntegrationAnswerRequest();
  IntegrationCallbackResult handleIntegrationHangupRequest();
  IntegrationCallbackResult handleIntegrationRingRequest(const String &pattern);
  IntegrationCallbackResult handleIntegrationCallWaitingRequest();
  void handleIntegrationCallBlocked(const String &number);
  void handleIntegrationMaintenanceModeChanged(const bool enabled);
  void handleIntegrationConfigChanged(ConfigChangeEvent event);

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
  void performFactoryReset();

  // Configuration change handlers (safe no-op via stub when integration off)
  void onAudioConfigChanged();
  void onMaintenanceModeChanged(const bool enabled);

  DeviceConfig _deviceConfig;
  State _state;

  Modem _modem;
  Ringer _ringer;
  HookSwitch _hookSwitch;
  RotaryDial _rotaryDial;
  Wifi _wifi;
  TimeManager _timeManager;
  NumberHandler _numberHandler;

  std::unique_ptr<IntegrationManager> _integrationManager;

  uint32_t _stateTime = 0UL;
  bool _firstTimeSystemReady = false;

  uint32_t _lastRinger = 0UL;
  uint32_t _lastTimeMgr = 0UL;
  uint32_t _lastWifi = 0UL;
  uint32_t _lastIntegration = 0UL;
};
