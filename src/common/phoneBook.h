#pragma once

#include <Arduino.h>

enum class DialedNumberValidationResult { Valid, Pending, Invalid };

DialedNumberValidationResult validateDialedNumber(const char *number);

// Phonebook functions that can work with either local or HA data
bool isPhoneBookEntry_Runtime(const char *number);
bool isPartialOfFullPhoneBookEntry_Runtime(const char *number);
const char *getPhoneBookNumberForEntry_Runtime(const char *entry);
