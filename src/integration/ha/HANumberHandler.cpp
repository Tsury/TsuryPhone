#ifdef HOME_ASSISTANT_INTEGRATION

#include "HANumberHandler.h"
#include "HAConfig.h"

HANumberHandler::HANumberHandler(HAConfig &haConfig) : _haConfig(haConfig) {}

bool HANumberHandler::isWebhookTrigger(const String &dialedNumber) const {
  return _haConfig.hasWebhookAction(dialedNumber);
}

String HANumberHandler::getWebhookId(const String &code) const {
  return _haConfig.getWebhookId(code);
}

bool HANumberHandler::isPartialWebhookMatch(const String &dialedNumber) const {
  // Check webhook actions for partial matches
  for (const auto &action : _haConfig.getWebhookActions()) {
    if (action.code.startsWith(dialedNumber)) {
      return true;
    }
  }
  return false;
}

std::vector<String> HANumberHandler::listFullCodes() const {
  std::vector<String> codes;
  codes.reserve(_haConfig.getWebhookActions().size());
  for (const auto &action : _haConfig.getWebhookActions()) {
    codes.push_back(action.code);
  }
  return codes;
}

#endif // HOME_ASSISTANT_INTEGRATION
