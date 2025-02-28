#include "hookSwitch.h"
#include "common/logger.h"
#include "config.h"

namespace {
  constexpr int kHookDebounce = 50;
}

HookSwitch::HookSwitch()
    : _state(HIGH), _statePrevious(HIGH), _stateChangeTime(0), _stateChanged(false) {}

void HookSwitch::init() const {
  Logger::infoln(F("Initializing hook switch..."));

  pinMode(kHookSwitchPin, INPUT_PULLUP);

  Logger::infoln(F("Hook switch initialized!"));
}

void HookSwitch::process() {
  int newState = digitalRead(kHookSwitchPin);

  if (newState != _statePrevious) {
    _stateChangeTime = millis();
  } else {
    // We must set to false here so that the two "justChanged" functions
    // can return true only once when the state changes.
    _stateChanged = false;
  }

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
  if (_stateChanged && isOffHook()) {
    // Clear the flag so that this won't return true again until the state
    // changes.
    // TODO: Is this bad practice?
    _stateChanged = false;
    return true;
  }

  return false;
}

bool HookSwitch::justChangedOnHook() {
  if (_stateChanged && isOnHook()) {
    _stateChanged = false;
    return true;
  }

  return false;
}