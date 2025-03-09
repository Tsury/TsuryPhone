#pragma once

#include <WiFiManager.h>

class Wifi {
public:
  void init();
  void process();

  void openConfigPortal();

private:
  void onWifiConnected();

#ifdef WEB_SERIAL
  void initWebSerial();
  void processWebSerial();
#endif

  WiFiManager _wifiManager;

#ifdef WEB_SERIAL
  unsigned long _lastWebSerialPrint = 0UL;
#endif
};