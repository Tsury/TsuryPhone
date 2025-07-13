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

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial();
#endif

  WiFiManager _wifiManager;
  std::function<void()> _configPortalTimeoutCallback;

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
#endif
};