#pragma once

#include "common/state.h"

class Ringer {
public:
  void init() const;
  void process(State &state);

  void startRinging();
  void startRinging(int durationMs); // Ring for specific duration
  void stopRinging();

private:
  void setRingerEnabled(const bool enabled) const;

  bool _ringing = false;
  bool _ringState = false;

  uint32_t _ringStartTime = 0UL;
  uint32_t _lastCycleTime = 0UL;
  int _customRingingDuration = 0; // Custom ring duration in ms, 0 = use default
};
