#pragma once

#include <WiFiManager.h>

class Wifi {
public:
  void init();
  void process();

private:
  WiFiManager _wifiManager;
};