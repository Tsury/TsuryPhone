#include "ringer.h"
#include "common/logger.h"
#include "config.h"
#include "core/DeviceConfig.h"
#include <Arduino.h>
#include <cstring>

namespace {
  const constexpr int kRingCycleDuration = 30;
  const constexpr uint32_t kDefaultRingDurationMs = 2000;
}

void Ringer::init() const {
  Logger::infoln(F("Initializing ringer..."));

  pinMode(kRingerIn1Pin, OUTPUT);
  pinMode(kRingerIn2Pin, OUTPUT);
  pinMode(kRingerInhPin, OUTPUT);

  setRingerEnabled(false);

  Logger::infoln(F("Ringer initialized!"));
}

void Ringer::startRinging() {
  startRinging(String(DeviceConfig::getDefaultRingPattern()));
}

void Ringer::startRinging(uint32_t duration) {
  if (_ringing) {
    return;
  }

  resetPattern();
  _pattern.durations.push_back(duration);
  _pattern.active = false;
  _ignoreDnd = false;
  initializeRinging();
}

void Ringer::startRinging(const String &pattern, bool ignoreDnd) {
  if (_ringing) {
    return;
  }

  resetPattern();
  parsePattern(pattern);
  _pattern.active = true;
  _ignoreDnd = ignoreDnd;
  initializeRinging();
}

void Ringer::process(State &state) {
  if (!_ringing) {
    return;
  }

  if (state.isDnd && !_ignoreDnd) {
    stopRinging();
    return;
  }

  if (_pattern.active) {
    // Pattern-based ringing logic
    if (_pattern.index >= _pattern.durations.size()) {
      // Pattern complete, check for repeats
      _pattern.currentRepeat++;

      if (_pattern.currentRepeat >= _pattern.repeatCount) {
        markRingComplete(state);
        return;
      }
      // Reset for next repeat
      _pattern.index = 0;
      _pattern.startTime = millis();
    }

    uint32_t currentDuration = _pattern.durations[_pattern.index];
    bool shouldRing = (_pattern.index % 2 == 0); // Ring on odd indices (0, 2, 4...)

    if (millis() - _pattern.startTime >= currentDuration) {
      _pattern.index++;
      _pattern.startTime = millis();

      if (_pattern.index < _pattern.durations.size()) {
        shouldRing = (_pattern.index % 2 == 0);
      }
    }

    // Handle ring state cycling for active ring periods
    if (shouldRing) {
      updateRingState();
    } else {
      // During wait periods, ensure ringer is off
      setRingerPins(false, false);
    }
  } else {
    // Original simple ringing logic
    if (millis() - _ringStartTime >= _pattern.durations[0]) {
      markRingComplete(state);
      return;
    }

    updateRingState();
  }
}

void Ringer::stopRinging() {
  if (!_ringing) {
    return;
  }

  _ringing = false;
  _ignoreDnd = false;
  setRingerEnabled(false);
  resetPattern();
}

void Ringer::markRingComplete(State &state) {
  stopRinging();

  // If we're encountering phantom caller announcements, wrap this with
  // "if (state.callState.active.isValid()) {"
  state.callState.active.rangAtLeastOnce = true;
}

void Ringer::setRingerEnabled(const bool enabled) const {
  digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

  if (!enabled) {
    setRingerPins(false, false);
  }
}

void Ringer::initializeRinging() {
  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _pattern.startTime = millis();
  _lastCycleTime = millis() + kRingCycleDuration;
  _ringState = false;
  _pattern.index = 0;
  _pattern.currentRepeat = 0;
}

void Ringer::updateRingState() {
  if (millis() - _lastCycleTime >= kRingCycleDuration) {
    _ringState = !_ringState;
    _lastCycleTime = millis();
    setRingerPins(_ringState, !_ringState);
  }
}

void Ringer::setRingerPins(bool pin1High, bool pin2High) const {
  digitalWrite(kRingerIn1Pin, pin1High ? HIGH : LOW);
  digitalWrite(kRingerIn2Pin, pin2High ? HIGH : LOW);
}

void Ringer::parsePattern(const String &pattern) {
  String patternToParse =
      pattern.length() == 0 ? String(DeviceConfig::getDefaultRingPattern()) : pattern;

  // Check if pattern is a simple numeric
  char *endPtr;
  long value = strtol(patternToParse.c_str(), &endPtr, 10);
  if (*endPtr == '\0' && value > 0) {
    // Simple numeric pattern
    _pattern.durations.push_back(static_cast<uint32_t>(value));
    return;
  }

  // Parse comma-separated pattern
  String patternCopy = patternToParse;

  // Check for repeat suffix (xNUM)
  int repeatPos = patternCopy.lastIndexOf('x');
  if (repeatPos != -1) {
    String repeatStr = patternCopy.substring(repeatPos + 1);
    _pattern.repeatCount = static_cast<uint32_t>(repeatStr.toInt());
    if (_pattern.repeatCount == 0) {
      _pattern.repeatCount = 1;
    }
    patternCopy = patternCopy.substring(0, repeatPos);
  }

  // Parse durations using indexOf instead of strtok
  int startPos = 0;
  int commaPos = 0;

  while ((commaPos = patternCopy.indexOf(',', startPos)) != -1) {
    String durationStr = patternCopy.substring(startPos, commaPos);
    long duration = durationStr.toInt();
    if (duration > 0) {
      _pattern.durations.push_back(static_cast<uint32_t>(duration));
    }
    startPos = commaPos + 1;
  }

  // Handle the last duration (after the last comma or the only one)
  if (startPos < patternCopy.length()) {
    String durationStr = patternCopy.substring(startPos);
    long duration = durationStr.toInt();
    if (duration > 0) {
      _pattern.durations.push_back(static_cast<uint32_t>(duration));
    }
  }

  // Validate pattern
  if (_pattern.durations.empty()) {
    _pattern.durations.push_back(kDefaultRingDurationMs);
    return;
  }

  // If repeat is specified, pattern must have even number of durations
  if (_pattern.repeatCount > 1 && (_pattern.durations.size() % 2 != 0)) {
    Logger::errorln(F("Pattern with repeats must have even number of durations"));
    _pattern.durations.clear();
    _pattern.durations.push_back(kDefaultRingDurationMs);
    _pattern.repeatCount = 1;
    return;
  }

  // If no repeat specified, pattern must have odd number of durations
  if (_pattern.repeatCount == 1 && (_pattern.durations.size() % 2 == 0)) {
    Logger::errorln(F("Pattern without repeats must have odd number of durations"));
    _pattern.durations.clear();
    _pattern.durations.push_back(kDefaultRingDurationMs);
    return;
  }
}

void Ringer::resetPattern() {
  _pattern.reset();
}