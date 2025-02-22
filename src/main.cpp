#include "main.h"
#include "common/common.h"
#include "common/helpers.h"
#include "common/logger.h"

#define BAUD_RATE MODEM_BAUDRATE
#define CHECK_HARDWARE_TIMEOUT 1500
#define CHECK_LINE_TIMEOUT 1500

PhoneApp::PhoneApp()
{
    // Constructor: initialize members if needed.
}

void PhoneApp::setup()
{
    Serial.begin(BAUD_RATE);
    Logger::info("TsuryPhone starting...");

    modem.init();
    ringer.init();
    rotaryDial.init();
    hookSwitch.init();

    setState(AppState::CheckHardware);
}

void PhoneApp::loop()
{
    // #if DEBUG
    //     if (SerialAT.available())
    //     {
    //         Serial.write(SerialAT.read());
    //     }
    //     if (Serial.available())
    //     {
    //         SerialAT.write(Serial.read());
    //     }

    //     return;
    // #endif

    processState();

    hookSwitch.process();
    rotaryDial.process();

    StateResult stateResult = modem.deriveNewStateFromMessage(state);
    modem.process(stateResult);

    if (stateResult.callerNumber.length() > 0)
    {
        Serial.print("We have caller number for state: ");
        Serial.println(appStateToString(stateResult.newState));

        Serial.print("Current app state: ");
        Serial.println(appStateToString(state));
    }

    if (stateResult.newState != state)
    {
        setState(stateResult.newState, stateResult);
    }
}

void PhoneApp::setState(AppState newState, const StateResult &result)
{
#if DEBUG
    if (state == newState)
    {
        Serial.printf("Retrying state %s\n", appStateToString(state));
    }
    else
    {
        Serial.printf("Changing state from %s to %s\n", appStateToString(state), appStateToString(newState));
    }
#endif

    state = newState;
    stateTime = millis();

    switch (newState)
    {
    case AppState::CheckHardware:
        onSetStateCheckHardwareState();
        break;
    case AppState::CheckLine:
        onSetStateCheckLineState();
        break;
    case AppState::Idle:
        onSetStateIdleState();
        break;
    case AppState::IncomingCall:
        onSetStateIncomingCallState(result);
        break;
    case AppState::IncomingCallRing:
        onSetStateIncomingCallRingState();
        break;
    case AppState::InCall:
        onSetStateInCallState();
        break;
    case AppState::Dialing:
        break;
    case AppState::WifiTool:
        break;
    default:
        break;
    }
}

void PhoneApp::onSetStateCheckHardwareState()
{
    modem.sendCheckHardwareCommand();
}

void PhoneApp::onSetStateCheckLineState()
{
    modem.sendCheckLineCommand();
}

void PhoneApp::onSetStateIdleState()
{
    if (firstTimeSystemReady)
    {
        return;
    }

    firstTimeSystemReady = true;
    modem.enqueueMp3(state_ready);
    modem.playTone(1, 10000);
    Serial.println("System ready!");
}

void PhoneApp::onSetStateIncomingCallState(const StateResult &result)
{
    if (hasMp3ForCaller(result.callerNumber))
    {
        Serial.print("Playing MP3 for caller: ");
        Serial.println(result.callerNumber);
        modem.setSpeakerVolume();
        modem.enqueueMp3(callerNumbersToMp3s[result.callerNumber.c_str()]);
        modem.setEarpieceVolume();
    }
    else
    {
        Serial.print("No MP3 for caller: ");
        Serial.println(result.callerNumber);
        // TODO: TTS?
    }
}

void PhoneApp::onSetStateIncomingCallRingState()
{
    Serial.println("Ringing...");
    ringer.startRinging();
}

void PhoneApp::onSetStateInCallState()
{
    modem.setEarpieceVolume();
}

void PhoneApp::processState()
{
    switch (state)
    {
    case AppState::CheckHardware:
        processStateCheckHardware();
        break;
    case AppState::CheckLine:
        processStateCheckLine();
        break;
    case AppState::Idle:
        processStateIdle();
        break;
    case AppState::InCall:
        processStateInCall();
    case AppState::Dialing:
        processStateDialing();
        break;
    case AppState::IncomingCall:
    case AppState::IncomingCallRing:
        processStateIncomingCall();
        break;
    case AppState::WifiTool:
        break;
    default:
        break;
    }
}

void PhoneApp::processStateCheckHardware()
{
    if (millis() - stateTime > CHECK_HARDWARE_TIMEOUT)
    {
        setState(AppState::CheckHardware);
    }
}

void PhoneApp::processStateCheckLine()
{
    if (millis() - stateTime > CHECK_LINE_TIMEOUT)
    {
        setState(AppState::CheckLine);
    }
}

void PhoneApp::processStateIdle()
{
    if (hookSwitch.justChangedOnHook())
    {
        rotaryDial.resetCurrentNumber();
        modem.stopPlaying();
    }

    if (hookSwitch.isOffHook())
    {
        DialedNumberResult dialedNumberResult = rotaryDial.getCurrentNumber();
        String dialedNumber = dialedNumberResult.callerNumber;

        if (dialedNumberResult.dialedDigit != 99)
        {
            Serial.print("Dialed digit: ");
            modem.enqueueMp3(dialedDigitsToMp3s[String(dialedNumberResult.dialedDigit).c_str()]);

            Serial.print("Dialed number: ");
            Serial.println(dialedNumber);
        }

        DialedNumberValidationResult dialedNumberValidation = validateDialedNumber(dialedNumber);

        if (dialedNumberValidation != DialedNumberValidationResult::Pending)
        {
            if (dialedNumberValidation == DialedNumberValidationResult::Valid)
            {
                modem.enqueueCall(dialedNumber);
                rotaryDial.resetCurrentNumber();
            }
            else
            {
                modem.enqueueMp3(dial_error, 10);
                rotaryDial.resetCurrentNumber();
            }
        }
    }
}

void PhoneApp::processStateIncomingCall()
{
    if (hookSwitch.justChangedOffHook())
    {
        modem.answer();
    }
}

void PhoneApp::processStateDialing()
{
    if (hookSwitch.justChangedOnHook())
    {
        modem.hangUp();
    }
}

void PhoneApp::processStateInCall()
{
    int dialedDigit = rotaryDial.getDialedDigit();

    if (dialedDigit == 1)
    {
        Serial.println("Toggling volume...");
        modem.toggleVolume();
    }
    else if (dialedDigit == 2)
    {
        Serial.println("Playing MP3...");
        modem.playTone(7, 10000);
    }

    rotaryDial.resetCurrentNumber();
}