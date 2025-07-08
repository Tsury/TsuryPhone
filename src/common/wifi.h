#pragma once

#include <WiFiManager.h>

class Wifi {
public:
  void init();
  void process();

  void openConfigPortal();
  void openConfigPortalAsync();
  bool isConfigPortalActive() const;

private:
  void onWifiConnected();
  String generateDeviceId();
  String generateWifiSsid();

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial();
#endif

  WiFiManager _wifiManager;
  bool _configPortalActive = false;
  bool _configPortalRequested = false;
  String _deviceId;
  String _wifiSsid;

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
#endif
};