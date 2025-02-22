#include "config.h"
#include "hookSwitch.h"
#include "common/logger.h"

#define HOOK_DEBOUNCE 50

HookSwitch::HookSwitch() : state(HIGH),
                           statePrevious(HIGH),
                           stateChangeTime(0),
                           stateChanged(false)
{
}

void HookSwitch::init()
{
    Logger::info("Initializing hook switch...");

    pinMode(HOOK_SWITCH_PIN, INPUT_PULLUP);

    Logger::info("Hook switch initialized!");
}

void HookSwitch::process()
{
    int newState = digitalRead(HOOK_SWITCH_PIN);

    if (newState != statePrevious)
    {
        stateChangeTime = millis();
    }
    else
    {
        // We must set to false here so that the two "justChanged" functions
        // can return true only once when the state changes.
        stateChanged = false;
    }

    if ((millis() - stateChangeTime) >= HOOK_DEBOUNCE)
    {
        if (newState != state)
        {
            state = newState;
            stateChanged = true;

            if (state == LOW)
            {
                Logger::info("Off hook!");
            }
            else
            {
                Logger::info("On hook!");
            }
        }
    }

    statePrevious = newState;
}

bool HookSwitch::isOffHook()
{
    return (state == LOW);
}

bool HookSwitch::isOnHook()
{
    return (state == HIGH);
}

bool HookSwitch::justChangedOffHook()
{
    if (stateChanged && isOffHook())
    {
        // Clear the flag so that this won't return true again until the state changes.
        // TODO: Is this bad practice?
        stateChanged = false;
        return true;
    }

    return false;
}

bool HookSwitch::justChangedOnHook()
{
    if (stateChanged && isOnHook())
    {
        stateChanged = false;
        return true;
    }

    return false;
}