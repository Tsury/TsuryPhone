#include "hookSwitch.h"
#include "common/logger.h"
#include "config.h"

namespace {
  constexpr int kHookDebounce = 50;
}

HookSwitch::HookSwitch()
    : state(HIGH), statePrevious(HIGH), stateChangeTime(0), stateChanged(false) {}

void HookSwitch::init() const {
  Logger::infoln(F("Initializing hook switch..."));

  pinMode(kHookSwitchPin, INPUT_PULLUP);

  Logger::infoln(F("Hook switch initialized!"));
}

void HookSwitch::process() {
  int newState = digitalRead(kHookSwitchPin);

  if (newState != statePrevious) {
    stateChangeTime = millis();
  } else {
    // We must set to false here so that the two "justChanged" functions
    // can return true only once when the state changes.
    stateChanged = false;
  }

  if ((millis() - stateChangeTime) >= kHookDebounce) {
    if (newState != state) {
      state = newState;
      stateChanged = true;

      if (state == LOW) {
        Logger::infoln(F("Off hook!"));
      } else {
        Logger::infoln(F("On hook!"));
      }
    }
  }

  statePrevious = newState;
}

bool HookSwitch::isOffHook() const {
  return (state == LOW);
}

bool HookSwitch::isOnHook() const {
  return (state == HIGH);
}

bool HookSwitch::justChangedOffHook() {
  if (stateChanged && isOffHook()) {
    // Clear the flag so that this won't return true again until the state
    // changes.
    // TODO: Is this bad practice?
    stateChanged = false;
    return true;
  }

  return false;
}

bool HookSwitch::justChangedOnHook() {
  if (stateChanged && isOnHook()) {
    stateChanged = false;
    return true;
  }

  return false;
}