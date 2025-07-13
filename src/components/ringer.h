#pragma once

#include "common/state.h"
#include <vector>

struct RingPattern {
  std::vector<int> timings;  // Ring on/off durations in ms
  int repeatCount;           // Number of times to repeat pattern
  
  RingPattern() : repeatCount(1) {}
};

class Ringer {
public:
  void init() const;
  void process(State &state);

  void startRinging();
  void startRinging(const String& pattern);
  void stopRinging();

private:
  void setRingerEnabled(const bool enabled) const;
  RingPattern parseRingPattern(const String& pattern);

  bool _ringing = false;
  bool _ringState = false;

  uint32_t _ringStartTime = 0UL;
  uint32_t _lastCycleTime = 0UL;
  
  // Pattern support
  RingPattern _currentPattern;
  int _currentPatternIndex = 0;
  int _currentRepeat = 0;
  bool _usingCustomPattern = false;
};
