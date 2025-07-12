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
  uint32_t _stateChangeTime = 0UL;
  bool _stateChanged = false;
};
