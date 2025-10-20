#include "phoneNormalization.h"

namespace {

  bool isFormattingChar(char c) {
    switch (c) {
    case ' ':
    case '-':
    case '(':
    case ')':
    case '.':
    case '\t':
    case '\r':
    case '\n':
      return true;
    default:
      return false;
    }
  }

  String stripFormatting(const String &input) {
    String cleaned;
    cleaned.reserve(input.length());
    for (size_t i = 0; i < input.length(); ++i) {
      char c = input.charAt(i);
      if (isFormattingChar(c)) {
        continue;
      }
      if (c == '+' && cleaned.isEmpty()) {
        cleaned += c;
        continue;
      }
      if (c >= '0' && c <= '9') {
        cleaned += c;
      }
    }
    return cleaned;
  }

  String stripLeadingZeros(const String &digits) {
    size_t idx = 0;
    while (idx + 1 < digits.length() && digits.charAt(idx) == '0') {
      ++idx;
    }
    if (idx == 0) {
      return digits;
    }
    if (idx >= digits.length()) {
      return String();
    }
    return digits.substring(idx);
  }

  String ensurePlusPrefix(const String &digits) {
    if (digits.isEmpty()) {
      return String();
    }
    if (digits.charAt(0) == '+') {
      return digits;
    }
    return String("+") + digits;
  }

  String normalizeDefaultCode(const String &code) {
    String sanitized;
    sanitized.reserve(code.length());
    for (size_t i = 0; i < code.length(); ++i) {
      char c = code.charAt(i);
      if (c >= '0' && c <= '9') {
        sanitized += c;
      }
    }
    sanitized = stripLeadingZeros(sanitized);
    return sanitized;
  }

  String digitsOnly(const String &value) {
    String digits;
    digits.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
      char c = value.charAt(i);
      if (c >= '0' && c <= '9') {
        digits += c;
      }
    }
    return digits;
  }

} // namespace

namespace PhoneNormalization {

  String stripToDigits(const String &value) {
    return digitsOnly(value);
  }

  String sanitizeDefaultDialingCode(const String &code) {
    return normalizeDefaultCode(code);
  }

  String normalizePhoneNumber(const String &rawNumber, const String &defaultDialingCode) {
    String trimmed = rawNumber;
    trimmed.trim();
    if (trimmed.isEmpty()) {
      return String();
    }

    String cleaned = stripFormatting(trimmed);
    if (cleaned.isEmpty()) {
      return String();
    }

    bool hasPlus = cleaned.charAt(0) == '+';
    String digits = hasPlus ? cleaned.substring(1) : cleaned;

    if (digits.isEmpty()) {
      return String();
    }

    if (hasPlus) {
      return ensurePlusPrefix(digits);
    }

    if (digits.startsWith("00") && digits.length() > 2) {
      String international = digits.substring(2);
      international = stripLeadingZeros(international);
      if (!international.isEmpty()) {
        return ensurePlusPrefix(international);
      }
    }

    String sanitizedCode = normalizeDefaultCode(defaultDialingCode);

    if (!sanitizedCode.isEmpty()) {
      if (digits.startsWith(sanitizedCode)) {
        return ensurePlusPrefix(digits);
      }
      if (digits.charAt(0) == '0' && digits.length() >= 7) {
        String local = digits.substring(1);
        local = stripLeadingZeros(local);
        if (!local.isEmpty()) {
          return ensurePlusPrefix(sanitizedCode + local);
        }
      }

      if (digits.length() >= 8) {
        return ensurePlusPrefix(sanitizedCode + digits);
      }
    }

    return digits;
  }

  bool numbersEquivalent(const String &lhs, const String &rhs, const String &defaultDialingCode) {
    if (lhs.equalsIgnoreCase(rhs)) {
      return true;
    }

    String normLhs = normalizePhoneNumber(lhs, defaultDialingCode);
    String normRhs = normalizePhoneNumber(rhs, defaultDialingCode);

    if (normLhs.isEmpty() || normRhs.isEmpty()) {
      return false;
    }

    return normLhs.equalsIgnoreCase(normRhs);
  }

} // namespace PhoneNormalization
