#pragma once

#include "state.h"
#include <Arduino.h>

class DeviceConfig;

class TimeManager {
public:
  TimeManager(DeviceConfig &config);
  void init() const;
  void process(State &state);

private:
  bool fetchLocalTime(struct tm &timeinfo) const;

  DeviceConfig &_config;
  uint32_t _lastDndCheckTime = 0UL;
};
