#include "ringer.h"
#include "common/logger.h"
#include "config.h"
#include <Arduino.h>
#include <cstring>

namespace {
  const constexpr int kRingCycleDuration = 30;
  const constexpr int kDefaultRingDuration = 2000;
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
  Logger::debugln(F("Ringer: startRinging() called - using default duration %d ms"),
                  kDefaultRingDuration);
  startRinging(kDefaultRingDuration);
}

void Ringer::startRinging(uint32_t duration) {
  Logger::debugln(F("Ringer: startRinging(duration=%d) called"), duration);

  if (_ringing) {
    Logger::debugln(F("Ringer: Already ringing, ignoring request"));
    return;
  }

  resetPattern();
  _pattern.durations.push_back(duration);
  _pattern.active = false;
  Logger::debugln(F("Ringer: Set up simple duration pattern: %d ms"), duration);
  initializeRinging();
}

void Ringer::startRinging(const String &pattern) {
  String logMsg = "Ringer: startRinging(pattern=" + pattern + ") called";
  Logger::debugln(logMsg.c_str());

  if (_ringing) {
    Logger::debugln(F("Ringer: Already ringing, ignoring pattern request"));
    return;
  }

  resetPattern();
  parsePattern(pattern);
  _pattern.active = true;

  // Log parsed pattern details
  String patternDetails = "Ringer: Parsed pattern - durations: ";
  for (size_t i = 0; i < _pattern.durations.size(); i++) {
    if (i > 0) {
      patternDetails += ",";
    }
    patternDetails += String(_pattern.durations[i]);
  }
  patternDetails += " repeats: " + String(_pattern.repeatCount);
  Logger::debugln(patternDetails.c_str());

  initializeRinging();
}

void Ringer::process(State &state) {
  if (!_ringing) {
    return;
  }

  if (state.isDnd) {
    Logger::debugln(F("Ringer: DND mode active, stopping ringing"));
    stopRinging();
    return;
  }

  if (_pattern.active) {
    // Pattern-based ringing logic
    if (_pattern.index >= _pattern.durations.size()) {
      // Pattern complete, check for repeats
      _pattern.currentRepeat++;
      Logger::debugln(F("Ringer: Pattern complete, repeat %d of %d"),
                      _pattern.currentRepeat,
                      _pattern.repeatCount);

      if (_pattern.currentRepeat >= _pattern.repeatCount) {
        Logger::debugln(F("Ringer: All repeats complete, stopping"));
        stopRinging();
        state.callState.rangAtLeastOnce = true;
        return;
      }
      // Reset for next repeat
      _pattern.index = 0;
      _pattern.startTime = millis();
      Logger::debugln(F("Ringer: Starting repeat %d"), _pattern.currentRepeat + 1);
    }

    uint32_t currentDuration = _pattern.durations[_pattern.index];
    bool shouldRing = (_pattern.index % 2 == 0);   // Ring on odd indices (0, 2, 4...)
    static bool lastShouldRing = !shouldRing;      // Track state changes
    static uint32_t lastPatternIndex = UINT32_MAX; // Track index changes

    if (millis() - _pattern.startTime >= currentDuration) {
      _pattern.index++;
      _pattern.startTime = millis();

      Logger::debugln(F("Ringer: Pattern phase %d complete (%d ms), moving to phase %d"),
                      _pattern.index - 1,
                      currentDuration,
                      _pattern.index);

      if (_pattern.index < _pattern.durations.size()) {
        shouldRing = (_pattern.index % 2 == 0);
        Logger::debugln(F("Ringer: Phase %d - %s for %d ms"),
                        _pattern.index,
                        shouldRing ? "RING" : "WAIT",
                        _pattern.durations[_pattern.index]);
      }
    }

    // Log state changes only
    if (shouldRing != lastShouldRing || _pattern.index != lastPatternIndex) {
      Logger::debugln(F("Ringer: State change - phase %d, %s"),
                      _pattern.index,
                      shouldRing ? "RINGING" : "WAITING");
      lastShouldRing = shouldRing;
      lastPatternIndex = _pattern.index;
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
    static bool simpleRingLogged = false;

    if (!simpleRingLogged) {
      Logger::debugln(F("Ringer: Simple ring mode - duration %d ms"), _pattern.durations[0]);
      simpleRingLogged = true;
    }

    if (millis() - _ringStartTime >= _pattern.durations[0]) {
      Logger::debugln(F("Ringer: Simple ring duration complete, stopping"));
      stopRinging();
      state.callState.rangAtLeastOnce = true;
      simpleRingLogged = false; // Reset for next time
      return;
    }

    updateRingState();
  }
}

void Ringer::stopRinging() {
  if (!_ringing) {
    Logger::debugln(F("Ringer: stopRinging() called but not currently ringing"));
    return;
  }

  Logger::debugln(F("Ringer: Stopping ringing"));
  _ringing = false;
  setRingerEnabled(false);
  resetPattern();
}

void Ringer::setRingerEnabled(const bool enabled) const {
  Logger::debugln(F("Ringer: Setting ringer %s"), enabled ? "ENABLED" : "DISABLED");
  digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

  if (!enabled) {
    setRingerPins(false, false);
  }
}

void Ringer::initializeRinging() {
  Logger::debugln(F("Ringer: Initializing ringing - pattern mode: %s"),
                  _pattern.active ? "PATTERN" : "SIMPLE");

  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _pattern.startTime = millis();
  _lastCycleTime = millis() + kRingCycleDuration;
  _ringState = false;
  _pattern.index = 0;
  _pattern.currentRepeat = 0;

  if (_pattern.active && !_pattern.durations.empty()) {
    Logger::debugln(F("Ringer: Starting pattern phase 0 - %s for %d ms"),
                    (_pattern.index % 2 == 0) ? "RING" : "WAIT",
                    _pattern.durations[0]);
  }
}

void Ringer::updateRingState() {
  if (millis() - _lastCycleTime >= kRingCycleDuration) {
    _ringState = !_ringState;
    _lastCycleTime = millis();
    setRingerPins(_ringState, !_ringState);

    // Only log ring state changes occasionally to avoid spam
    static uint32_t lastLogTime = 0;
    if (millis() - lastLogTime > 1000) { // Log every 1 second max
      Logger::debugln(F("Ringer: Ring cycle - pin states: IN1=%s, IN2=%s"),
                      _ringState ? "HIGH" : "LOW",
                      !_ringState ? "HIGH" : "LOW");
      lastLogTime = millis();
    }
  }
}

void Ringer::setRingerPins(bool pin1High, bool pin2High) const {
  static bool lastPin1State = false;
  static bool lastPin2State = false;

  // Only log when pin states actually change
  if (pin1High != lastPin1State || pin2High != lastPin2State) {
    Logger::debugln(F("Ringer: Pin change - IN1: %s->%s, IN2: %s->%s"),
                    lastPin1State ? "HIGH" : "LOW",
                    pin1High ? "HIGH" : "LOW",
                    lastPin2State ? "HIGH" : "LOW",
                    pin2High ? "HIGH" : "LOW");
    lastPin1State = pin1High;
    lastPin2State = pin2High;
  }

  digitalWrite(kRingerIn1Pin, pin1High ? HIGH : LOW);
  digitalWrite(kRingerIn2Pin, pin2High ? HIGH : LOW);
}

void Ringer::parsePattern(const String &pattern) {
  String logMsg = "Ringer: Parsing pattern: " + pattern;
  Logger::debugln(logMsg.c_str());

  if (pattern.length() == 0) {
    Logger::debugln(F("Ringer: Empty pattern, using default duration %d ms"), kDefaultRingDuration);
    _pattern.durations.push_back(kDefaultRingDuration);
    return;
  }

  // Check if pattern is a simple numeric
  char *endPtr;
  long value = strtol(pattern.c_str(), &endPtr, 10);
  if (*endPtr == '\0' && value > 0) {
    // Simple numeric pattern
    Logger::debugln(F("Ringer: Simple numeric pattern detected: %d ms"), value);
    _pattern.durations.push_back(static_cast<uint32_t>(value));
    return;
  }

  // Parse comma-separated pattern
  String patternCopy = pattern;
  Logger::debugln(F("Ringer: Parsing comma-separated pattern"));

  // Check for repeat suffix (xNUM)
  int repeatPos = patternCopy.lastIndexOf('x');
  if (repeatPos != -1) {
    String repeatStr = patternCopy.substring(repeatPos + 1);
    _pattern.repeatCount = static_cast<uint32_t>(repeatStr.toInt());
    if (_pattern.repeatCount == 0) {
      _pattern.repeatCount = 1;
    }
    patternCopy = patternCopy.substring(0, repeatPos);
    Logger::debugln(F("Ringer: Repeat count found: %d"), _pattern.repeatCount);
  }

  // Parse durations using indexOf instead of strtok
  int startPos = 0;
  int commaPos = 0;
  int durationCount = 0;

  while ((commaPos = patternCopy.indexOf(',', startPos)) != -1) {
    String durationStr = patternCopy.substring(startPos, commaPos);
    long duration = durationStr.toInt();
    if (duration > 0) {
      _pattern.durations.push_back(static_cast<uint32_t>(duration));
      Logger::debugln(F("Ringer: Duration %d: %d ms"), durationCount++, duration);
    }
    startPos = commaPos + 1;
  }

  // Handle the last duration (after the last comma or the only one)
  if (startPos < patternCopy.length()) {
    String durationStr = patternCopy.substring(startPos);
    long duration = durationStr.toInt();
    if (duration > 0) {
      _pattern.durations.push_back(static_cast<uint32_t>(duration));
      Logger::debugln(F("Ringer: Duration %d: %d ms"), durationCount++, duration);
    }
  }

  // Validate pattern
  if (_pattern.durations.empty()) {
    Logger::debugln(F("Ringer: No valid durations found, using default"));
    _pattern.durations.push_back(kDefaultRingDuration);
    return;
  }

  Logger::debugln(F("Ringer: Pattern validation - %d durations, %d repeats"),
                  _pattern.durations.size(),
                  _pattern.repeatCount);

  // If repeat is specified, pattern must have even number of durations
  if (_pattern.repeatCount > 1 && (_pattern.durations.size() % 2 != 0)) {
    Logger::errorln(F("Pattern with repeats must have even number of durations"));
    Logger::debugln(F("Ringer: Validation failed - odd durations with repeats"));
    _pattern.durations.clear();
    _pattern.durations.push_back(kDefaultRingDuration);
    _pattern.repeatCount = 1;
    return;
  }

  // If no repeat specified, pattern must have odd number of durations
  if (_pattern.repeatCount == 1 && (_pattern.durations.size() % 2 == 0)) {
    Logger::errorln(F("Pattern without repeats must have odd number of durations"));
    Logger::debugln(F("Ringer: Validation failed - even durations without repeats"));
    _pattern.durations.clear();
    _pattern.durations.push_back(kDefaultRingDuration);
    return;
  }

  Logger::debugln(F("Ringer: Pattern validation passed"));
}

void Ringer::resetPattern() {
  Logger::debugln(F("Ringer: Resetting pattern"));
  _pattern.reset();
}