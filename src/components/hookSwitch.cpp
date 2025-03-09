#include "hookSwitch.h"
#include "common/logger.h"
#include "config.h"

namespace {
  const constexpr int kHookDebounce = 50;
}

HookSwitch::HookSwitch()
    : _state(HIGH), _statePrevious(HIGH), _stateChangeTime(0UL), _stateChanged(false) {}

void HookSwitch::init() const {
  Logger::infoln(F("Initializing hook switch..."));

  pinMode(kHookSwitchPin, INPUT_PULLUP);

  Logger::infoln(F("Hook switch initialized!"));
}

void HookSwitch::process() {
  int newState = digitalRead(kHookSwitchPin);

  if (newState != _statePrevious) {
    _stateChangeTime = millis();
  }

  // We must set to false here so that the two "justChanged" functions
  // can return true only once, exactly when the state changes.
  _stateChanged = false;

  if ((millis() - _stateChangeTime) >= kHookDebounce) {
    if (newState != _state) {
      _state = newState;
      _stateChanged = true;

      if (_state == LOW) {
        Logger::infoln(F("Off hook!"));
      } else {
        Logger::infoln(F("On hook!"));
      }
    }
  }

  _statePrevious = newState;
}

bool HookSwitch::isOffHook() const {
  return (_state == LOW);
}

bool HookSwitch::isOnHook() const {
  return (_state == HIGH);
}

bool HookSwitch::justChangedOffHook() {
  return _stateChanged && isOffHook();
}

bool HookSwitch::justChangedOnHook() {
  return _stateChanged && isOnHook();
}