#include "config.h"
#include "rotaryDial.h"
#include "Arduino.h"
#include "common/logger.h"

namespace
{
    constexpr int kRotaryDebounce = 10;
}

void RotaryDial::init() const
{
    Logger::infoln(F("Initializing rotary dial..."));

    pinMode(kRotaryDialInDialPin, INPUT_PULLUP);
    pinMode(kRotaryDialPulsePin, INPUT_PULLUP);

    Logger::infoln(F("Rotary dial initialized!"));
}

void RotaryDial::process()
{
    _dialedDigit = 99;

    int newInDialedState = digitalRead(kRotaryDialInDialPin);

    if (newInDialedState != inDialPreviousState)
    {
        inDialChangeTime = millis();
    }

    if ((millis() - inDialChangeTime) >= kRotaryDebounce)
    {
        if (newInDialedState != inDialState)
        {
            inDialState = newInDialedState;

            if (inDialState == LOW)
            {
                Logger::infoln(F("Start of dial"));
                counter = 0;
            }
            else
            {
                Logger::infoln(F("End of dial"));

                if (counter > 0)
                {
                    if (counter == 10)
                    {
                        counter = 0;
                    }

                    _dialedDigit = counter;
                }
            }
        }
    }

    inDialPreviousState = newInDialedState;

    int newPulseState = digitalRead(kRotaryDialPulsePin);

    if (newPulseState != pulsePreviousState)
    {
        pulseChangeTime = millis();
    }

    if ((millis() - pulseChangeTime) >= kRotaryDebounce)
    {
        if (newPulseState != pulseState)
        {
            pulseState = newPulseState;

            if (pulseState == LOW)
            {
                counter++;
            }
        }
    }

    pulsePreviousState = newPulseState;

    if (_dialedDigit != 99)
    {
        Logger::infoln(F("Dialed digit: %d"), _dialedDigit);
    }
}

int RotaryDial::getDialedDigit() const
{
    return _dialedDigit;
}

DialedNumberResult RotaryDial::getCurrentNumber()
{
    DialedNumberResult res = {"", 99};

    int dialedDigit = getDialedDigit();

    if (dialedDigit != 99)
    {
        size_t len = strlen(currentNumber);

        if (len < sizeof(currentNumber) - 1)
        {
            currentNumber[len] = '0' + dialedDigit;
            currentNumber[len + 1] = '\0';
        }

        res.dialedDigit = dialedDigit;
    }

    snprintf(res.callerNumber, sizeof(res.callerNumber), "%s", currentNumber);

    return res;
}

void RotaryDial::resetCurrentNumber()
{
    currentNumber[0] = '\0';
}