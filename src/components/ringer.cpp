#include "ringer.h"
#include "common/logger.h"
#include "config.h"
#include <Arduino.h>

namespace {
  const constexpr int kRingCycleDuration = 30;
  const constexpr int kRingDuration = 2000;
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
  _customRingingDuration = 0; // Use default duration
  _usingPattern = false;
}

void Ringer::startRinging(int durationMs) {
  if (_ringing) {
    return;
  }

  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _lastCycleTime = millis() + kRingCycleDuration;
  _ringState = false;
  _customRingingDuration = durationMs;
  _usingPattern = false;
}

void Ringer::startRingingWithPattern(const char* pattern) {
  if (_ringing) {
    return;
  }

  _currentPattern = parseRingPattern(pattern);
  if (!_currentPattern.isValid) {
    Logger::errorln(F("Invalid ring pattern: %s"), pattern);
    // Fall back to default ringing
    startRinging();
    return;
  }

  Logger::infoln(F("Starting ring pattern: %s"), pattern);
  
  setRingerEnabled(true);
  _ringing = true;
  _ringStartTime = millis();
  _lastCycleTime = millis();
  _ringState = false;
  _usingPattern = true;
  _currentPatternIndex = 0;
  _currentPatternRepeat = 0;
  
  // Start with first duration (should be a ring)
  if (_currentPattern.durations.size() > 0) {
    _ringState = true; // Start ringing
  }
}

RingPattern Ringer::parseRingPattern(const char* pattern) {
  RingPattern rp;
  String patternStr(pattern);
  
  // Check for repeat syntax: pattern/num
  int slashPos = patternStr.indexOf('/');
  String mainPattern = patternStr;
  int repeats = 1;
  
  if (slashPos > 0) {
    mainPattern = patternStr.substring(0, slashPos);
    repeats = patternStr.substring(slashPos + 1).toInt();
    if (repeats <= 0) repeats = 1;
  }
  
  // Parse comma-separated durations
  int startPos = 0;
  int commaPos;
  std::vector<int> durations;
  
  do {
    commaPos = mainPattern.indexOf(',', startPos);
    String durStr = (commaPos > 0) ? mainPattern.substring(startPos, commaPos) : mainPattern.substring(startPos);
    int duration = durStr.toInt();
    
    if (duration <= 0 || duration > 30000) { // Max 30 seconds per duration
      return rp; // Invalid
    }
    
    durations.push_back(duration);
    startPos = commaPos + 1;
  } while (commaPos > 0);
  
  // Validate pattern
  if (durations.empty()) {
    return rp; // Invalid
  }
  
  // If there are repeats, pattern must end with even number (pause)
  if (repeats > 1 && durations.size() % 2 != 0) {
    return rp; // Invalid
  }
  
  rp.durations = durations;
  rp.repeats = repeats;
  rp.isValid = true;
  
  return rp;
}

void Ringer::process(State &state) {
  if (!_ringing) {
    return;
  }

  if (state.isDnd) {
    stopRinging();
    return;
  }

  if (_usingPattern) {
    // Pattern-based ringing logic
    if (_currentPatternIndex >= _currentPattern.durations.size()) {
      // Pattern completed, check if we need to repeat
      _currentPatternRepeat++;
      if (_currentPatternRepeat >= _currentPattern.repeats) {
        // All repeats done
        stopRinging();
        state.callState.rangAtLeastOnce = true;
        return;
      } else {
        // Start next repeat
        _currentPatternIndex = 0;
        _lastCycleTime = millis();
        _ringState = true; // Start with ring
      }
    }

    // Check if current duration has elapsed
    int currentDuration = _currentPattern.durations[_currentPatternIndex];
    if (millis() - _lastCycleTime >= currentDuration) {
      // Move to next duration
      _currentPatternIndex++;
      _lastCycleTime = millis();
      
      if (_currentPatternIndex < _currentPattern.durations.size()) {
        // Odd indices (0, 2, 4...) are ring durations
        // Even indices (1, 3, 5...) are pause durations
        _ringState = (_currentPatternIndex % 2 == 0);
      }
    }

    // Apply current ring state
    if (_ringState) {
      if (millis() - _lastCycleTime < kRingCycleDuration || (millis() - _lastCycleTime) % (kRingCycleDuration * 2) < kRingCycleDuration) {
        digitalWrite(kRingerIn1Pin, HIGH);
        digitalWrite(kRingerIn2Pin, LOW);
      } else {
        digitalWrite(kRingerIn1Pin, LOW);
        digitalWrite(kRingerIn2Pin, HIGH);
      }
    } else {
      // Pause - turn off ringer
      setRingerEnabled(false);
    }
  } else {
    // Traditional duration-based ringing logic
    int ringDuration = (_customRingingDuration > 0) ? _customRingingDuration : kRingDuration;

    if (millis() - _ringStartTime >= ringDuration) {
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
  }
}

void Ringer::stopRinging() {
  if (!_ringing) {
    return;
  }

  _ringing = false;
  _usingPattern = false;
  setRingerEnabled(false);
}

void Ringer::setRingerEnabled(const bool enabled) const {
  digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

  if (!enabled) {
    digitalWrite(kRingerIn1Pin, LOW);
    digitalWrite(kRingerIn2Pin, LOW);
  }
}
