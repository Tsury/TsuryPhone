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

void RotaryDial::process(State &state) {
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

          // Add the dialed digit to the state's current dialing number
          size_t len = strlen(state.currentDialingNumber);
          if (len < sizeof(state.currentDialingNumber) - 1) {
            state.currentDialingNumber[len] = '0' + _dialedDigit;
            state.currentDialingNumber[len + 1] = '\0';
          }
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