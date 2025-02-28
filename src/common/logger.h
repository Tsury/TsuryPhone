#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="

#include "common.h"
#include <Arduino.h>
#include <stdio.h>

enum class LogLevel { Debug = 0, Info, Warn, Error };

namespace Logger {
  extern LogLevel currentLogLevel;

  inline void setLogLevel(const LogLevel level) {
    currentLogLevel = level;
  }

  template <typename... Args>
  inline void logln(const __FlashStringHelper *prefix, const char *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf(buffer, sizeof(buffer), format, args...);
    Serial.printf("%S %s\n", prefix, buffer);
  }

  template <typename... Args>
  inline void log(const char *prefix, const char *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf(buffer, sizeof(buffer), format, args...);
    Serial.printf("%s %s", prefix, buffer);
  }

  template <typename... Args>
  inline void
  logln(const __FlashStringHelper *prefix, const __FlashStringHelper *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf_P(buffer, sizeof(buffer), reinterpret_cast<PGM_P>(format), args...);
    Serial.printf("%S %s\n", prefix, buffer);
  }

  template <typename... Args>
  inline void log(const char *prefix, const __FlashStringHelper *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf_P(buffer, sizeof(buffer), reinterpret_cast<PGM_P>(format), args...);
    Serial.printf("%s %s", prefix, buffer);
  }

  template <typename... Args> inline void debugln(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Debug) {
      logln(F("[DEBUG]"), format, args...);
    }
  }
  template <typename... Args> inline void debug(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Debug) {
      log(F("[DEBUG]"), format, args...);
    }
  }
  template <typename... Args>
  inline void debugln(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Debug) {
      logln(F("[DEBUG]"), format, args...);
    }
  }
  template <typename... Args>
  inline void debug(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Debug) {
      log(F("[DEBUG]"), format, args...);
    }
  }

  template <typename... Args> inline void infoln(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Info) {
      logln(F("[INFO]"), format, args...);
    }
  }
  template <typename... Args> inline void info(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Info) {
      log(F("[INFO]"), format, args...);
    }
  }
  template <typename... Args>
  inline void infoln(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Info) {
      logln(F("[INFO]"), format, args...);
    }
  }
  template <typename... Args>
  inline void info(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Info) {
      log(F("[INFO]"), format, args...);
    }
  }

  template <typename... Args> inline void warnln(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Warn) {
      logln(F("[WARN]"), format, args...);
    }
  }
  template <typename... Args> inline void warn(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Warn) {
      log(F("[WARN]"), format, args...);
    }
  }
  template <typename... Args>
  inline void warnln(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Warn) {
      logln(F("[WARN]"), format, args...);
    }
  }
  template <typename... Args>
  inline void warn(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Warn) {
      log(F("[WARN]"), format, args...);
    }
  }

  template <typename... Args> inline void errorln(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Error) {
      logln(F("[ERROR]"), format, args...);
    }
  }
  template <typename... Args> inline void error(const char *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Error) {
      log(F("[ERROR]"), format, args...);
    }
  }
  template <typename... Args>
  inline void errorln(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Error) {
      logln(F("[ERROR]"), format, args...);
    }
  }
  template <typename... Args>
  inline void error(const __FlashStringHelper *format, const Args... args) {
    if (currentLogLevel <= LogLevel::Error) {
      log(F("[ERROR]"), format, args...);
    }
  }
}

#pragma GCC diagnostic pop