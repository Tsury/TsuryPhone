#pragma once

#include "config/corePhoneConfig.h"
#include "core/phoneController.h"
#include "hardware/hookSwitch.h"
#include "hardware/modem.h"
#include "hardware/ringer.h"
#include "hardware/rotaryDial.h"
#include "network/timeManager.h"
#include "network/wifi.h"
#include "servers/server.h"
#include "servers/serverManager.h"
#include "utils/consts.h"
#include <Arduino.h>
#include <memory>

// Forward declaration
class PhoneApp;

class PhoneApp : public IPhoneController {
public:
  PhoneApp();
  ~PhoneApp();

  void setup();
  void loop();

  // IPhoneController interface implementation
  void performCall(const char *number) override;
  void performHangup() override;
  void performReset() override;
  void performRingWithStructuredPattern(const RingPattern &pattern) override;
  void setDndForceEnabled(bool enabled) override;
  void setDndScheduleEnabled(bool enabled) override;
  void setDndHours(int startHour, int startMinute, int endHour, int endMinute) override;
  void performSetMaintenanceMode(bool enabled) override;
  void performSwitchToCallWaiting() override;
  void addQuickDialEntry(const char *name, const char *number) override;
  void removeQuickDialEntry(const char *name) override;

private:
  void setupServers();
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
  ServerManager _serverManager;
  State _state{};
  std::shared_ptr<CorePhoneConfig> _corePhoneConfig;

  uint32_t _stateTime = 0UL;
  bool _firstTimeSystemReady = false;
};
