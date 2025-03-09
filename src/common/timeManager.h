#pragma once

#include "state.h"
#include <Arduino.h>

class TimeManager {
public:
  void init() const;
  void process(State &state);

private:
  bool fetchLocalTime(struct tm &timeinfo) const;

  uint32_t _lastDndCheckTime = 0UL;
};
