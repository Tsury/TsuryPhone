#include "rotaryDial.h"
#include "common/logger.h"
#include "config.h"

namespace {
  const constexpr int kRotaryDebounce = 10;
}

void RotaryDial::init() const {
  Logger::infoln(F("Initializing rotary dial..."));

  pinMode(kRotaryDialInDialPin, INPUT_PULLUP);
  pinMode(kRotaryDialPulsePin, INPUT_PULLUP);

  Logger::infoln(F("Rotary dial initialized!"));
}

void RotaryDial::process() {
  _dialedDigit = kInvalidDialedDigit;

  int newInDialedState = digitalRead(kRotaryDialInDialPin);

  if (newInDialedState != _inDialPreviousState) {
    _inDialChangeTime = millis();
  }

  if ((millis() - _inDialChangeTime) >= kRotaryDebounce) {
    if (newInDialedState != _inDialState) {
      _inDialState = newInDialedState;

      if (_inDialState == LOW) {
        Logger::infoln(F("Start of dial"));
        _counter = 0;
      } else {
        Logger::infoln(F("End of dial"));

        if (_counter > 0) {
          if (_counter == 10) {
            _counter = 0;
          }

          _dialedDigit = _counter;
        }
      }
    }
  }

  _inDialPreviousState = newInDialedState;

  int newPulseState = digitalRead(kRotaryDialPulsePin);

  if (newPulseState != _pulsePreviousState) {
    _pulseChangeTime = millis();
  }

  if ((millis() - _pulseChangeTime) >= kRotaryDebounce) {
    if (newPulseState != _pulseState) {
      _pulseState = newPulseState;

      if (_pulseState == LOW) {
        _counter++;
      }
    }
  }

  _pulsePreviousState = newPulseState;

  if (_dialedDigit != kInvalidDialedDigit) {
    Logger::infoln(F("Dialed digit: %d"), _dialedDigit);
  }
}

int RotaryDial::getDialedDigit() const {
  return _dialedDigit;
}

DialedNumberResult RotaryDial::getCurrentNumber() {
  DialedNumberResult res = {"", kInvalidDialedDigit};

  const int dialedDigit = getDialedDigit();

  if (dialedDigit != kInvalidDialedDigit) {
    size_t len = strlen(_currentNumber);

    if (len < sizeof(_currentNumber) - 1) {
      _currentNumber[len] = '0' + dialedDigit;
      _currentNumber[len + 1] = '\0';
    }

    res.dialedDigit = dialedDigit;
  }

  snprintf(res.callNumber, sizeof(res.callNumber), "%s", _currentNumber);

  return res;
}

void RotaryDial::resetCurrentNumber() {
  _currentNumber[0] = '\0';
}