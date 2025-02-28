#pragma once

#include <Arduino.h>

class HookSwitch {
public:
  HookSwitch();

  void init() const;
  void process();

  bool isOffHook() const;
  bool isOnHook() const;
  bool justChangedOffHook();
  bool justChangedOnHook();

private:
  int _state = HIGH;
  int _statePrevious = HIGH;
  unsigned long _stateChangeTime = 0;
  bool _stateChanged = false;
};
