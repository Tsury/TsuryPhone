#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <ArduinoJson.h>
#include <functional>

// Config change event types
enum class ConfigChangeEvent {
  DND_CONFIG_CHANGED,
  AUDIO_CONFIG_CHANGED,
  QUICK_DIAL_CHANGED,
  BLOCKED_NUMBER_CHANGED,
  RING_PATTERN_CHANGED,
  DEFAULT_DIALING_CODE_CHANGED,
  INTEGRATION_EXTENSION_CHANGED // Generic bucket for integration-specific config changes
};

// Result structure for integration callbacks
struct IntegrationCallbackResult {
  bool success = false;
  String errorMessage;
  String errorCode; // DS5 structured error code
  JsonDocument data;

  IntegrationCallbackResult() = default;
  IntegrationCallbackResult(bool success) : success(success) {}
  IntegrationCallbackResult(bool success, const String &error)
      : success(success), errorMessage(error) {}
  IntegrationCallbackResult(bool success, const String &error, const String &code)
      : success(success), errorMessage(error), errorCode(code) {}
  IntegrationCallbackResult(bool success, const JsonDocument &resultData)
      : success(success), data(resultData) {}
  IntegrationCallbackResult(bool success, const String &error, const JsonDocument &resultData)
      : success(success), errorMessage(error), data(resultData) {}
  IntegrationCallbackResult(bool success,
                            const String &error,
                            const String &code,
                            const JsonDocument &resultData)
      : success(success), errorMessage(error), errorCode(code), data(resultData) {}
};

// Callback function type for config changes
using ConfigChangeCallback = std::function<void(ConfigChangeEvent event)>;

#endif // HOME_ASSISTANT_INTEGRATION