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
  int state = HIGH;
  int statePrevious = HIGH;
  unsigned long stateChangeTime = 0;
  bool stateChanged = false;
};
