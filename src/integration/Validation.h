#pragma once

#if defined(HOME_ASSISTANT_INTEGRATION) || defined(ANDROID_INTEGRATION)
#include <Arduino.h>

namespace IntegrationValidation {

  // Validate integration/action codes: ^[A-Za-z0-9_]{1,16}$
  inline bool isValidCode(const String &code) {
    size_t len = code.length();
    if (len == 0 || len > 16) {
      return false;
    }
    for (size_t i = 0; i < len; ++i) {
      char c = code[i];
      if (!(c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'))) {
        return false;
      }
    }
    return true;
  }

  // Validate phone number: ^[0-9+\-]{1,24}$
  inline bool isValidNumber(const String &num) {
    size_t len = num.length();
    if (len == 0 || len > 24) {
      return false;
    }
    for (size_t i = 0; i < len; ++i) {
      char c = num[i];
      if (!(c == '+' || c == '-' || (c >= '0' && c <= '9'))) {
        return false;
      }
    }
    return true;
  }

  // Basic URL validation: starts with http:// or https:// and has at least one '.' after scheme
  inline bool isValidUrl(const String &url) {
    if (!(url.startsWith("http://") || url.startsWith("https://"))) {
      return false;
    }
    int idx = url.indexOf('.', url.startsWith("https://") ? 8 : 7);
    return idx > 0 && idx < (int)url.length() - 1;
  }

  // Validate ring pattern: Non-empty, <=32 chars, digits separated by commas, optional x<repeat>
  // Accept forms like "500" "500,500,250" "500,500,500x3".
  inline bool isValidPattern(const String &pattern) {
    size_t len = pattern.length();
    if (len == 0 || len > 32) {
      return false;
    }
    bool sawDigit = false;
    bool afterX = false;
    for (size_t i = 0; i < len; ++i) {
      char c = pattern[i];
      if (c >= '0' && c <= '9') {
        sawDigit = true;
      } else if (c == ',') {
        if (afterX) {
          return false; // no commas after repeat segment
        }
        // require that previous char wasn't comma or 'x'
        if (i == 0) {
          return false;
        }
        char prev = pattern[i - 1];
        if (!(prev >= '0' && prev <= '9')) {
          return false;
        }
      } else if (c == 'x') {
        if (afterX) {
          return false; // only one x allowed
        }
        // previous must be digit
        if (i == 0) {
          return false;
        }
        char prev = pattern[i - 1];
        if (!(prev >= '0' && prev <= '9')) {
          return false;
        }
        afterX = true;
      } else {
        return false;
      }
    }
    // last char must be digit
    char last = pattern[len - 1];
    if (!(last >= '0' && last <= '9')) {
      return false;
    }
    return sawDigit;
  }

}

#endif
