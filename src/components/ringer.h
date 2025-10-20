#pragma once

#include "common/state.h"
#include <vector>

struct RingPattern {
  std::vector<uint32_t> durations;
  uint32_t index = 0;
  uint32_t startTime = 0UL;
  uint32_t repeatCount = 1;
  uint32_t currentRepeat = 0;
  bool active = false;

  void reset() {
    durations.clear();
    index = 0;
    startTime = 0UL;
    repeatCount = 1;
    currentRepeat = 0;
    active = false;
  }
};

class Ringer {
public:
  void init() const;
  void process(State &state);

  void startRinging();
  void startRinging(uint32_t duration);
  void startRinging(const String &pattern, bool ignoreDnd = false);
  void stopRinging();

private:
  void setRingerEnabled(const bool enabled) const;
  void parsePattern(const String &pattern);
  void resetPattern();
  void initializeRinging();
  void updateRingState();
  void setRingerPins(bool pin1High, bool pin2High) const;

  bool _ringing = false;
  bool _ringState = false;
  bool _ignoreDnd = false;

  uint32_t _ringStartTime = 0UL;
  uint32_t _lastCycleTime = 0UL;

  // Pattern support
  RingPattern _pattern;
};
