#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../core/NumberHandler.h"
#include "../ActionHandler.h"
#include <Arduino.h>

// Forward declarations
class HAConfig;

/**
 * Home Assistant specific number handler
 * Handles webhook triggers and other HA-specific number actions
 */
class HANumberHandler : public IIntegrationActionHandler {
public:
  HANumberHandler(HAConfig &haConfig);

  // Check if a dialed number is a webhook trigger
  bool isWebhookTrigger(const String &dialedNumber) const;

  // Get webhook ID for a code
  String getWebhookId(const String &code) const;

  // Check for partial webhook matches (for pending state)
  bool isPartialWebhookMatch(const String &dialedNumber) const;

  // IIntegrationActionHandler implementation
  bool isPartialMatch(const String &dialed) const override {
    return isPartialWebhookMatch(dialed);
  }
  bool isFullMatch(const String &dialed) const override {
    return isWebhookTrigger(dialed);
  }
  String resolveActionId(const String &dialed) const override {
    return getWebhookId(dialed);
  }
  std::vector<String> listFullCodes() const override;

private:
  HAConfig &_haConfig;
};

#endif // HOME_ASSISTANT_INTEGRATION
