#include "NumberHandler.h"
#include "../common/string.h"
#include "DeviceConfig.h"
#include "config.h"
#include <cstring>

NumberHandler::NumberHandler(DeviceConfig &config) : _config(config) {}

NumberValidationResult NumberHandler::validateNumber(const char *dialedNumber) {
  NumberValidationResult result;
  result.action = NumberAction::Invalid;
  result.isComplete = false;

  int len = strlen(dialedNumber);

  if (len == 0) {
    result.action = NumberAction::Pending;
    return result;
  }

  // Check for system numbers (reset, wifi portal)
  if (isSystemNumber(dialedNumber)) {
    result.action = NumberAction::SystemAction;
    result.targetNumber = String(dialedNumber);
    result.isComplete = true;
    return result;
  }

  // Check for exact quick dial match
  if (_config.hasQuickDialEntry(String(dialedNumber))) {
    result.action = NumberAction::QuickDial;
    result.targetNumber = _config.getQuickDialNumber(String(dialedNumber));
    result.isComplete = true;
    return result;
  }

  // Check for exact webhook action match
  if (_config.hasWebhookAction(String(dialedNumber))) {
    result.action = NumberAction::WebhookTrigger;
    result.webhookId = _config.getWebhookId(String(dialedNumber));
    result.isComplete = true;
    return result;
  }

  // Check if this could be a partial match for quick dial or webhook
  if (isPartialMatch(dialedNumber)) {
    result.action = NumberAction::Pending;
    return result;
  }

  // Check phone number patterns
  DialedNumberValidationResult phoneValidation = validatePhonePattern(dialedNumber);

  switch (phoneValidation) {
  case DialedNumberValidationResult::Valid:
    result.action = NumberAction::DirectDial;
    result.targetNumber = String(dialedNumber);
    result.isComplete = true;
    break;

  case DialedNumberValidationResult::Pending:
    result.action = NumberAction::Pending;
    break;

  case DialedNumberValidationResult::Invalid:
  default:
    result.action = NumberAction::Invalid;
    result.isComplete = true;
    break;
  }

  return result;
}

bool NumberHandler::isPartialMatch(const char *dialedNumber) {
  String dialedStr(dialedNumber);

  // Check quick dial entries for partial matches
  for (const auto &entry : _config.getQuickDialEntries()) {
    if (entry.code.startsWith(dialedStr)) {
      return true;
    }
  }

  // Check webhook actions for partial matches
  for (const auto &action : _config.getWebhookActions()) {
    if (action.code.startsWith(dialedStr)) {
      return true;
    }
  }

  // Check system numbers for partial matches
  for (size_t i = 0; i < kSystemNumbersCount; i++) {
    if (strStartsWith(kSystemNumbers[i], dialedStr.c_str())) {
      return true;
    }
  }

  return false;
}

bool NumberHandler::isSystemNumber(const char *number) {
  // Check against all system numbers
  for (size_t i = 0; i < kSystemNumbersCount; i++) {
    if (strEqual(number, kSystemNumbers[i])) {
      return true;
    }
  }
  return false;
}

DialedNumberValidationResult NumberHandler::validatePhonePattern(const char *number) {
  // Use the existing phone pattern validation logic
  return validateDialedNumber(number);
}
