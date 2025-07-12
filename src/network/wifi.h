#pragma once

#include <WiFiManager.h>

class Wifi {
public:
  void init();
  void process();

  void openConfigPortal();
  void openConfigPortalAsync();
  void closeConfigPortal();
  bool isConfigPortalActive() const;
  void setMaintenanceModeController(std::function<void(bool)> controller);

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
  bool _manuallyClosing = false;
  std::function<void(bool)> _maintenanceModeController;
  String _deviceId;
  String _wifiSsid;

#ifdef WEB_SERIAL
  uint32_t _lastWebSerialPrint = 0UL;
#endif
};
