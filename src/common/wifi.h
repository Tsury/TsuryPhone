#pragma once

#include <WiFiManager.h>
#include <functional>

class Wifi {
public:
  void init();
  void process();

  void openConfigPortal();
  void closeConfigPortal();
  bool isConfigPortalActive();
  void setConfigPortalTimeoutCallback(std::function<void()> callback);

private:
  void onWifiConnected();
  void onConfigPortalTimeout();
  void processConfigPortal();

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial();
#endif

  WiFiManager _wifiManager;
  std::function<void()> _configPortalTimeoutCallback;

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