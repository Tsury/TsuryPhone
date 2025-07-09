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

void Ringer::startRingingWithStructuredPattern(const RingPattern &pattern) {
  if (_ringing) {
    return;
  }

  if (!pattern.isValid) {
    Logger::errorln(F("Invalid structured ring pattern"));
    // Fall back to default ringing
    startRinging();
    return;
  }

  _currentPattern = pattern;
  Logger::infoln(F("Starting structured ring pattern: %d durations, %d repeats"),
                 pattern.durations.size(),
                 pattern.repeats);

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
    // Check if current duration has elapsed
    if (_currentPatternIndex < _currentPattern.durations.size()) {
      int currentDuration = _currentPattern.durations[_currentPatternIndex];
      if (millis() - _lastCycleTime >= currentDuration) {
        // Move to next duration
        _currentPatternIndex++;
        _lastCycleTime = millis();

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
          }
        }

        // Update ring state based on current index
        // Even indices (0, 2, 4...) are ring durations
        // Odd indices (1, 3, 5...) are pause durations
        _ringState = (_currentPatternIndex % 2 == 0);
      }
    }

    // Apply current ring state
    if (_ringState) {
      // Ring phase - alternate the ringer pins
      if (millis() - _lastCycleTime < kRingCycleDuration ||
          (millis() - _lastCycleTime) % (kRingCycleDuration * 2) < kRingCycleDuration) {
        digitalWrite(kRingerIn1Pin, HIGH);
        digitalWrite(kRingerIn2Pin, LOW);
      } else {
        digitalWrite(kRingerIn1Pin, LOW);
        digitalWrite(kRingerIn2Pin, HIGH);
      }
    } else {
      // Pause phase - turn off ringer pins but keep ringer enabled
      digitalWrite(kRingerIn1Pin, LOW);
      digitalWrite(kRingerIn2Pin, LOW);
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
