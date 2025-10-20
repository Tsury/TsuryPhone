#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../common/logger.h"
#include <Arduino.h>

// Integration structured logging macros (C4)
// Define ENABLE_INT_LOG_DEBUG to enable debug level.

// Runtime toggle (defaults true in debug builds, false otherwise)
inline bool &integrationDebugEnabled() {
  static bool enabled =
#ifdef ENABLE_INT_LOG_DEBUG
      true;
#else
      false;
#endif
  return enabled;
}

inline void setIntegrationDebugLogging(bool enabled) {
  integrationDebugEnabled() = enabled;
}

#define INT_LOG_DEBUG(tag, fmt, ...)                                                               \
  do {                                                                                             \
    if (integrationDebugEnabled()) {                                                               \
      Logger::debugln(F("[%s][D] " fmt), tag, ##__VA_ARGS__);                                      \
    }                                                                                              \
  } while (0)

#define INT_LOG_INFO(tag, fmt, ...) Logger::infoln(F("[%s][I] " fmt), tag, ##__VA_ARGS__)
#define INT_LOG_WARN(tag, fmt, ...) Logger::warnln(F("[%s][W] " fmt), tag, ##__VA_ARGS__)
#define INT_LOG_ERROR(tag, fmt, ...) Logger::errorln(F("[%s][E] " fmt), tag, ##__VA_ARGS__)

// Convenience wrappers when an integration instance pointer `this` is available.
#define INTL_INFO(fmt, ...) INT_LOG_INFO(getTag(), fmt, ##__VA_ARGS__)
#define INTL_WARN(fmt, ...) INT_LOG_WARN(getTag(), fmt, ##__VA_ARGS__)
#define INTL_ERROR(fmt, ...) INT_LOG_ERROR(getTag(), fmt, ##__VA_ARGS__)
#define INTL_DEBUG(fmt, ...) INT_LOG_DEBUG(getTag(), fmt, ##__VA_ARGS__)

#endif // HOME_ASSISTANT_INTEGRATION