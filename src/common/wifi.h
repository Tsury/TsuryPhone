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
  // after seconds. Returns false when a suspension is already requested or running.
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
  void processSuspend();

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

  // Requested suspension in seconds (set by the web server task, taken by process()).
  std::atomic<uint32_t> _suspendRequestSeconds{0};
  uint32_t _suspendRequestedAt = 0UL;
  bool _suspendPending = false;
  bool _suspended = false;
  uint32_t _suspendStart = 0UL;
  uint32_t _suspendDurationMs = 0UL;

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
  bool _webSerialActive = false;
#endif
};