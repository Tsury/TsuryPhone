#pragma once

#include "common/state.h"

class Ringer {
public:
  void init() const;
  void process(State &state);

  void startRinging();
  void stopRinging();

private:
  void setRingerEnabled(const bool enabled) const;

  bool _ringing = false;
  bool _ringState = false;

  uint32_t _ringStartTime = 0UL;
  uint32_t _lastCycleTime = 0UL;
};
