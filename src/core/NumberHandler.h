#pragma once

#include "../common/phoneBook.h"
#include <Arduino.h>

class DeviceConfig;

enum class NumberAction {
  Invalid,
  Pending,
  QuickDial,
  WebhookTrigger,
  DirectDial,
  Blocked,
  SystemAction
};

struct NumberValidationResult {
  NumberAction action;
  String targetNumber; // For QuickDial and DirectDial
  String webhookId;    // For WebhookTrigger
  bool isComplete;     // Whether dialing is complete for this number
};

class NumberHandler {
public:
  NumberHandler(DeviceConfig &config);

  NumberValidationResult validateNumber(const char *dialedNumber);
  bool isPartialMatch(const char *dialedNumber);

private:
  bool isSystemNumber(const char *number);
  bool matchesPhonePattern(const char *number);
  DialedNumberValidationResult validatePhonePattern(const char *number);

  DeviceConfig &_config;
};
