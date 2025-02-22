#include "config.h"
#include "rotaryDial.h"
#include "Arduino.h"
#include "common/logger.h"

#define DEBOUNCE 10

RotaryDial::RotaryDial()
{
}

void RotaryDial::init()
{
    Logger::info("Initializing rotary dial...");

    pinMode(ROTARY_DIAL_IN_DIAL_PIN, INPUT_PULLUP);
    pinMode(ROTARY_DIAL_PULSE_PIN, INPUT_PULLUP);

    Logger::info("Rotary dial initialized!");
}

void RotaryDial::process()
{
    _dialedDigit = 99;

    int newInDialedState = digitalRead(ROTARY_DIAL_IN_DIAL_PIN);

    if (newInDialedState != inDialPreviousState)
    {
        inDialChangeTime = millis();
    }

    if ((millis() - inDialChangeTime) >= DEBOUNCE)
    {
        if (newInDialedState != inDialState)
        {
            inDialState = newInDialedState;

            if (inDialState == LOW)
            {
                Logger::info("Start of dial");
                counter = 0;
            }
            else
            {
                Logger::info("End of dial");

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

    int newPulseState = digitalRead(ROTARY_DIAL_PULSE_PIN);

    if (newPulseState != pulsePreviousState)
    {
        pulseChangeTime = millis();
    }

    if ((millis() - pulseChangeTime) >= DEBOUNCE)
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
        Logger::info("Dialed digit: " + String(_dialedDigit));
    }
}

int RotaryDial::getDialedDigit()
{
    return _dialedDigit;
}

DialedNumberResult RotaryDial::getCurrentNumber()
{
    DialedNumberResult res = {"", 99};

    int dialedDigit = getDialedDigit();

    if (dialedDigit != 99)
    {
        currentNumber += String(dialedDigit);
        res.dialedDigit = dialedDigit;
    }

    res.callerNumber = currentNumber;

    return res;
}

void RotaryDial::resetCurrentNumber()
{
    currentNumber = "";
}