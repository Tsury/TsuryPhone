#pragma once

#include "common/state.h"
#include <vector>

struct RingPattern {
  std::vector<int> durations;  // Alternating ring/pause durations in ms
  int repeats = 1;             // Number of times to repeat pattern
  bool isValid = false;
};

class Ringer {
public:
  void init() const;
  void process(State &state);

  void startRinging();
  void startRinging(int durationMs); // Ring for specific duration
  void startRingingWithStructuredPattern(const RingPattern& pattern); // Ring with pre-parsed pattern
  void stopRinging();

private:
  void setRingerEnabled(const bool enabled) const;

  bool _ringing = false;
  bool _ringState = false;

  uint32_t _ringStartTime = 0UL;
  uint32_t _lastCycleTime = 0UL;
  int _customRingingDuration = 0; // Custom ring duration in ms, 0 = use default
  
  // Pattern support
  RingPattern _currentPattern;
  int _currentPatternIndex = 0;
  int _currentPatternRepeat = 0;
  bool _usingPattern = false;
};
