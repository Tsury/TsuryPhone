#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION
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

  // Validate webhook id: allow broader character set used by HA webhooks
  // Accept 1-64 characters containing letters, digits, hyphen or underscore
  inline bool isValidWebhookId(const String &id) {
    size_t len = id.length();
    if (len == 0 || len > 64) {
      return false;
    }
    for (size_t i = 0; i < len; ++i) {
      char c = id[i];
      if (!(c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
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
    if (len == 0) {
      // Empty string defers to device default pattern
      return true;
    }
    if (len > 32) {
      return false;
    }

    bool sawDigit = false;
    bool afterX = false;
    int xIndex = -1;
    for (size_t i = 0; i < len; ++i) {
      char c = pattern[i];
      if (c >= '0' && c <= '9') {
        sawDigit = true;
      } else if (c == ',') {
        if (afterX) {
          return false; // no commas after repeat segment
        }
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
        if (i == 0) {
          return false;
        }
        char prev = pattern[i - 1];
        if (!(prev >= '0' && prev <= '9')) {
          return false;
        }
        afterX = true;
        xIndex = static_cast<int>(i);
      } else {
        return false;
      }
    }

    char last = pattern[len - 1];
    if (!(last >= '0' && last <= '9')) {
      return false;
    }

    long repeatCount = 1;
    if (xIndex != -1) {
      String repeatStr = pattern.substring(xIndex + 1);
      if (repeatStr.length() == 0) {
        return false;
      }
      repeatCount = repeatStr.toInt();
      if (repeatCount <= 0) {
        repeatCount = 1;
      }
    }

    String base = (xIndex != -1) ? pattern.substring(0, xIndex) : pattern;
    if (base.length() == 0) {
      return false;
    }

    int segments = 0;
    int start = 0;
    while (start < base.length()) {
      int comma = base.indexOf(',', start);
      int end = (comma == -1) ? base.length() : comma;
      if (end <= start) {
        return false;
      }
      String token = base.substring(start, end);
      long duration = token.toInt();
      if (duration <= 0) {
        return false;
      }
      ++segments;
      if (comma == -1) {
        break;
      }
      start = end + 1;
    }

    if (segments == 0) {
      return false;
    }

    if (repeatCount > 1) {
      if (segments % 2 != 0) {
        return false;
      }
    } else {
      if (segments % 2 == 0) {
        return false;
      }
    }

    return sawDigit;
  }

}

#endif // HOME_ASSISTANT_INTEGRATION
