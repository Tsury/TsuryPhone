#pragma once

#include "../core/DeviceConfig.h" // Include DeviceConfig definition
#include "state.h"
#include <WiFiManager.h>
#include <atomic>
#include <functional>

class Wifi {
public:
  Wifi(DeviceConfig &config);

  void init();
  void process(State &state);

  void openConfigPortal();
  void closeConfigPortal();
  bool isConfigPortalActive();

  void resetCredentials();

  // Thread-safe. Disconnects station Wi-Fi shortly after the call and reconnects by itself
  // after seconds on its own task. Returns false when a suspension is already running or its
  // task cannot start.
  bool requestSuspend(uint32_t seconds);

  // Set callback for when config portal times out (to exit maintenance mode)
  void setPortalTimeoutCallback(std::function<void()> callback) {
    _portalTimeoutCallback = callback;
  }

private:
  void onWifiConnected();
  void onConfigPortalTimeout();
  void processConfigPortal();
  void startConfigPortalSession(uint32_t timeoutMs);
  void onConfigPortalClosed();
  static void suspendTask(void *arg);

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial(State &state);
#endif

  DeviceConfig &_config;
  WiFiManager _wifiManager;
  std::function<void()> _portalTimeoutCallback;

  // Config portal state management
  bool _configPortalOpenRequested = false;
  bool _configPortalCloseRequested = false;
  bool _configPortalActive = false;
  uint32_t _configPortalStartTime = 0UL;
  uint32_t _configPortalTimeout = 0UL;

  // Seconds of the running suspension, 0 when none. Owned by suspendTask while non-zero.
  std::atomic<uint32_t> _suspendSeconds{0};

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
  bool _webSerialActive = false;
#endif
};