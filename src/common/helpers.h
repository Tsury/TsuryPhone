#pragma once

#include "Arduino.h"

enum class DialedNumberValidationResult
{
    Valid,   // Number is complete and fits one of the patterns.
    Pending, // Number is incomplete but could become valid.
    Invalid  // Number cannot possibly become valid.
};

DialedNumberValidationResult validateDialedNumber(const String &number);
