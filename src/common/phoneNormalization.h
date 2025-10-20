#pragma once

#include <Arduino.h>

namespace PhoneNormalization {

  String sanitizeDefaultDialingCode(const String &code);

  String normalizePhoneNumber(const String &rawNumber, const String &defaultDialingCode);

  bool numbersEquivalent(const String &lhs, const String &rhs, const String &defaultDialingCode);

  String stripToDigits(const String &value);

} // namespace PhoneNormalization
