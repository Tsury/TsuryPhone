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

  unsigned long _ringStartTime = 0UL;
  unsigned long _lastCycleTime = 0UL;
};
