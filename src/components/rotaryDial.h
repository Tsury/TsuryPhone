#pragma once

#include <Arduino.h>

struct DialedNumberResult
{
    String callerNumber;
    int dialedDigit;
};

class RotaryDial
{
public:
    RotaryDial();

    void init();
    void process();

    void resetCurrentNumber();
    int getDialedDigit();
    DialedNumberResult getCurrentNumber();

private:
    int inDialState = HIGH;
    int inDialPreviousState = HIGH;
    int pulseState = HIGH;
    int pulsePreviousState = HIGH;

    unsigned long inDialChangeTime = 0;
    unsigned long pulseChangeTime = 0;

    int counter = 0;

    char _dialedDigit = 99;

    String currentNumber = "";
};
