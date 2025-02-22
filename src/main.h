#pragma once

#include "Arduino.h"
#include "common\common.h"
#include "components\modem.h"
#include "components\ringer.h"
#include "components\hookSwitch.h"
#include "components\rotaryDial.h"

class PhoneApp
{
public:
    PhoneApp();

    // Initialize the application (call modem.init(), etc.)
    void setup();

    // Main loop processing for the application.
    void loop();

private:
    // Processes incoming messages from the modem.
    void processModemMessage();

    // Changes the state of the application.
    void setState(AppState newState, const StateResult &result = StateResult());

    // Processes the current state of the application.
    void processState();

    void processStateCheckHardware();

    void processStateCheckLine();

    void processStateIdle();

    void processStateIncomingCall();

    void processStateDialing();

    void processStateInCall();

    // --

    void onSetStateCheckHardwareState();

    void onSetStateCheckLineState();

    void onSetStateIdleState();

    void onSetStateIncomingCallState(const StateResult &result);

    void onSetStateIncomingCallRingState();

    void onSetStateInCallState();

    // The modem instance used by the application.
    Modem modem;

    Ringer ringer;

    HookSwitch hookSwitch;

    RotaryDial rotaryDial;

    // The current state of the application.
    AppState state = AppState::Startup;

    // The time the current state was entered.
    unsigned long stateTime = 0;

    bool firstTimeSystemReady = false;
};
