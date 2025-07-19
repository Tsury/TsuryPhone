#pragma once

#include "../core/DeviceConfig.h" // Include DeviceConfig definition
#include <WiFiManager.h>
#include <functional>

class Wifi {
public:
  Wifi(DeviceConfig &config);

  void init();
  void process();

  void openConfigPortal();
  void closeConfigPortal();
  bool isConfigPortalActive();

  // Set callback for when config portal times out (to exit maintenance mode)
  void setPortalTimeoutCallback(std::function<void()> callback) {
    _portalTimeoutCallback = callback;
  }

private:
  void onWifiConnected();
  void onConfigPortalTimeout();
  void processConfigPortal();

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial();
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

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
#endif
};