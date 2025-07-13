#include "ringer.h"
#include "common/logger.h"
#include "config.h"
#include <Arduino.h>

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
  if (_ringing) {
    return;
  }

  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _lastCycleTime = millis() + kRingCycleDuration;
  _ringState = false;
  _usingCustomPattern = false;
}

void Ringer::startRinging(const String& pattern) {
  if (_ringing) {
    return;
  }

  _currentPattern = parseRingPattern(pattern);
  _currentPatternIndex = 0;
  _currentRepeat = 0;
  _usingCustomPattern = true;
  
  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _lastCycleTime = millis();
  _ringState = true; // Start with ring on
  
  Logger::infoln(F("Starting pattern ringing: %s"), pattern.c_str());
}

RingPattern Ringer::parseRingPattern(const String& pattern) {
  RingPattern result;
  
  // Default pattern if parsing fails
  if (pattern.isEmpty()) {
    result.timings = {500, 500, 500, 500};
    result.repeatCount = 3;
    return result;
  }
  
  String workPattern = pattern;
  
  // Extract repeat count (e.g., "x3")
  int xPos = workPattern.indexOf('x');
  if (xPos >= 0) {
    String repeatStr = workPattern.substring(xPos + 1);
    result.repeatCount = repeatStr.toInt();
    if (result.repeatCount <= 0) result.repeatCount = 1;
    workPattern = workPattern.substring(0, xPos);
  } else {
    result.repeatCount = 1;
  }
  
  // Parse comma-separated timings
  int startPos = 0;
  int commaPos = workPattern.indexOf(',');
  
  while (commaPos >= 0 || startPos < workPattern.length()) {
    String timingStr;
    if (commaPos >= 0) {
      timingStr = workPattern.substring(startPos, commaPos);
      startPos = commaPos + 1;
      commaPos = workPattern.indexOf(',', startPos);
    } else {
      timingStr = workPattern.substring(startPos);
      startPos = workPattern.length();
    }
    
    int timing = timingStr.toInt();
    if (timing > 0) {
      result.timings.push_back(timing);
    }
  }
  
  // Ensure we have at least one timing
  if (result.timings.empty()) {
    result.timings = {500, 500};
  }
  
  return result;
}

void Ringer::process(State &state) {
  if (!_ringing) {
    return;
  }

  if (state.isDnd) {
    stopRinging();
    return;
  }

  if (!_usingCustomPattern) {
    // Original simple ringing logic
    if (millis() - _ringStartTime >= kDefaultRingDuration) {
      stopRinging();
      state.callState.rangAtLeastOnce = true;
      return;
    }

    if (millis() - _lastCycleTime >= kRingCycleDuration) {
      _ringState = !_ringState;
      _lastCycleTime = millis();

      if (_ringState) {
        digitalWrite(kRingerIn1Pin, HIGH);
        digitalWrite(kRingerIn2Pin, LOW);
      } else {
        digitalWrite(kRingerIn1Pin, LOW);
        digitalWrite(kRingerIn2Pin, HIGH);
      }
    }
  } else {
    // Pattern-based ringing
    if (_currentPatternIndex >= _currentPattern.timings.size()) {
      // Finished one cycle of the pattern
      _currentRepeat++;
      if (_currentRepeat >= _currentPattern.repeatCount) {
        // Finished all repeats
        stopRinging();
        state.callState.rangAtLeastOnce = true;
        return;
      }
      // Start next repeat
      _currentPatternIndex = 0;
      _lastCycleTime = millis();
    }
    
    // Check if current timing segment is complete
    if (millis() - _lastCycleTime >= _currentPattern.timings[_currentPatternIndex]) {
      _currentPatternIndex++;
      _lastCycleTime = millis();
      _ringState = !_ringState; // Toggle ring state for next segment
    }
    
    // Apply current ring state to hardware
    if (_ringState) {
      digitalWrite(kRingerIn1Pin, HIGH);
      digitalWrite(kRingerIn2Pin, LOW);
    } else {
      digitalWrite(kRingerIn1Pin, LOW);
      digitalWrite(kRingerIn2Pin, HIGH);
    }
  }
}

void Ringer::stopRinging() {
  if (!_ringing) {
    return;
  }

  _ringing = false;
  _usingCustomPattern = false;
  setRingerEnabled(false);
}

void Ringer::setRingerEnabled(const bool enabled) const {
  digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

  if (!enabled) {
    digitalWrite(kRingerIn1Pin, LOW);
    digitalWrite(kRingerIn2Pin, LOW);
  }
}