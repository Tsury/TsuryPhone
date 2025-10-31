#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="

#include "consts.h"
#include <Arduino.h>
#include <stdio.h>

#ifdef WEB_SERIAL
#include <WebSerial.h>
#endif

enum class LogLevel { Debug = 0, Info, Warn, Error };

#ifndef LOG_LEVEL_DEBUG
#define LOG_LEVEL_DEBUG 0
#endif
#ifndef LOG_LEVEL_INFO
#define LOG_LEVEL_INFO 1
#endif
#ifndef LOG_LEVEL_WARN
#define LOG_LEVEL_WARN 2
#endif
#ifndef LOG_LEVEL_ERROR
#define LOG_LEVEL_ERROR 3
#endif

#ifndef LOGGER_COMPILED_LEVEL
#define LOGGER_COMPILED_LEVEL LOG_LEVEL_DEBUG
#endif

#ifndef LOGGER_DEFAULT_LEVEL
#define LOGGER_DEFAULT_LEVEL LOGGER_COMPILED_LEVEL
#endif

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

#ifdef WEB_SERIAL
    WebSerial.printf("%S %s\n", prefix, buffer);
#endif
  }

  template <typename... Args>
  inline void log(const char *prefix, const char *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf(buffer, sizeof(buffer), format, args...);
    Serial.printf("%s %s", prefix, buffer);

#ifdef WEB_SERIAL
    WebSerial.printf("%s %s", prefix, buffer);
#endif
  }

  template <typename... Args>
  inline void
  logln(const __FlashStringHelper *prefix, const __FlashStringHelper *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf_P(buffer, sizeof(buffer), reinterpret_cast<PGM_P>(format), args...);
    Serial.printf("%S %s\n", prefix, buffer);

#ifdef WEB_SERIAL
    WebSerial.printf("%S %s\n", prefix, buffer);
#endif
  }

  template <typename... Args>
  inline void log(const char *prefix, const __FlashStringHelper *format, const Args... args) {
    char buffer[kBigBufferSize];
    snprintf_P(buffer, sizeof(buffer), reinterpret_cast<PGM_P>(format), args...);
    Serial.printf("%s %s", prefix, buffer);

#ifdef WEB_SERIAL
    WebSerial.printf("%s %s", prefix, buffer);
#endif
  }

  template <typename... Args> inline void debugln(const char *format, const Args... args);
  template <typename... Args> inline void debug(const char *format, const Args... args);
  template <typename... Args>
  inline void debugln(const __FlashStringHelper *format, const Args... args);
  template <typename... Args>
  inline void debug(const __FlashStringHelper *format, const Args... args);

  template <typename... Args> inline void infoln(const char *format, const Args... args);
  template <typename... Args> inline void info(const char *format, const Args... args);
  template <typename... Args>
  inline void infoln(const __FlashStringHelper *format, const Args... args);
  template <typename... Args>
  inline void info(const __FlashStringHelper *format, const Args... args);

  template <typename... Args> inline void warnln(const char *format, const Args... args);
  template <typename... Args> inline void warn(const char *format, const Args... args);
  template <typename... Args>
  inline void warnln(const __FlashStringHelper *format, const Args... args);
  template <typename... Args>
  inline void warn(const __FlashStringHelper *format, const Args... args);

  template <typename... Args> inline void errorln(const char *format, const Args... args);
  template <typename... Args> inline void error(const char *format, const Args... args);
  template <typename... Args>
  inline void errorln(const __FlashStringHelper *format, const Args... args);
  template <typename... Args>
  inline void error(const __FlashStringHelper *format, const Args... args);
}

#if LOGGER_COMPILED_LEVEL <= LOG_LEVEL_DEBUG
template <typename... Args> inline void Logger::debugln(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Debug) {
    logln(F("[DEBUG]"), format, args...);
  }
}
template <typename... Args> inline void Logger::debug(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Debug) {
    log(F("[DEBUG]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::debugln(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Debug) {
    logln(F("[DEBUG]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::debug(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Debug) {
    log(F("[DEBUG]"), format, args...);
  }
}
#else
template <typename... Args> inline void Logger::debugln(const char *, const Args...) {}
template <typename... Args> inline void Logger::debug(const char *, const Args...) {}
template <typename... Args>
inline void Logger::debugln(const __FlashStringHelper *, const Args...) {}
template <typename... Args> inline void Logger::debug(const __FlashStringHelper *, const Args...) {}
#endif

#if LOGGER_COMPILED_LEVEL <= LOG_LEVEL_INFO
template <typename... Args> inline void Logger::infoln(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Info) {
    logln(F("[INFO]"), format, args...);
  }
}
template <typename... Args> inline void Logger::info(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Info) {
    log(F("[INFO]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::infoln(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Info) {
    logln(F("[INFO]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::info(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Info) {
    log(F("[INFO]"), format, args...);
  }
}
#else
template <typename... Args> inline void Logger::infoln(const char *, const Args...) {}
template <typename... Args> inline void Logger::info(const char *, const Args...) {}
template <typename... Args>
inline void Logger::infoln(const __FlashStringHelper *, const Args...) {}
template <typename... Args> inline void Logger::info(const __FlashStringHelper *, const Args...) {}
#endif

#if LOGGER_COMPILED_LEVEL <= LOG_LEVEL_WARN
template <typename... Args> inline void Logger::warnln(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Warn) {
    logln(F("[WARN]"), format, args...);
  }
}
template <typename... Args> inline void Logger::warn(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Warn) {
    log(F("[WARN]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::warnln(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Warn) {
    logln(F("[WARN]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::warn(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Warn) {
    log(F("[WARN]"), format, args...);
  }
}
#else
template <typename... Args> inline void Logger::warnln(const char *, const Args...) {}
template <typename... Args> inline void Logger::warn(const char *, const Args...) {}
template <typename... Args>
inline void Logger::warnln(const __FlashStringHelper *, const Args...) {}
template <typename... Args> inline void Logger::warn(const __FlashStringHelper *, const Args...) {}
#endif

#if LOGGER_COMPILED_LEVEL <= LOG_LEVEL_ERROR
template <typename... Args> inline void Logger::errorln(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Error) {
    logln(F("[ERROR]"), format, args...);
  }
}
template <typename... Args> inline void Logger::error(const char *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Error) {
    log(F("[ERROR]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::errorln(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Error) {
    logln(F("[ERROR]"), format, args...);
  }
}
template <typename... Args>
inline void Logger::error(const __FlashStringHelper *format, const Args... args) {
  if (currentLogLevel <= LogLevel::Error) {
    log(F("[ERROR]"), format, args...);
  }
}
#else
template <typename... Args> inline void Logger::errorln(const char *, const Args...) {}
template <typename... Args> inline void Logger::error(const char *, const Args...) {}
template <typename... Args>
inline void Logger::errorln(const __FlashStringHelper *, const Args...) {}
template <typename... Args> inline void Logger::error(const __FlashStringHelper *, const Args...) {}
#endif

#pragma GCC diagnostic pop