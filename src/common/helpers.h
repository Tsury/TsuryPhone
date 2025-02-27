#pragma once

#include "Arduino.h"

enum class DialedNumberValidationResult { Valid, Pending, Invalid };

DialedNumberValidationResult validateDialedNumber(const char *number);
