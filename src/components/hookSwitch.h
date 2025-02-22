#pragma once

#include <Arduino.h>

class HookSwitch
{
public:
    HookSwitch();

    void init();

    void process();

    bool isOffHook();
    bool isOnHook();
    bool justChangedOffHook();
    bool justChangedOnHook();

private:
    unsigned long stateChangeTime = 0;

    int state = HIGH;
    int statePrevious = HIGH;

    bool stateChanged = false;
};
