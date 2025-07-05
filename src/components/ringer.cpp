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
}

void Ringer::process(State &state) {
  if (!_ringing) {
    return;
  }

  // if (state.isDnd) {
  //   stopRinging();
  //   return;
  // }

  // Use custom duration if set, otherwise use default
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

void Ringer::stopRinging() {
  if (!_ringing) {
    return;
  }

  _ringing = false;
  setRingerEnabled(false);
}

void Ringer::setRingerEnabled(const bool enabled) const {
  digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

  if (!enabled) {
    digitalWrite(kRingerIn1Pin, LOW);
    digitalWrite(kRingerIn2Pin, LOW);
  }
}