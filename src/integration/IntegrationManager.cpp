#include "IntegrationManager.h"
#include "../common/logger.h"

// Include available integrations
#ifdef HOME_ASSISTANT_INTEGRATION
#include "ha/HAIntegration.h"
#endif

IntegrationManager::IntegrationManager(DeviceConfig &config, DeviceStats &stats)
    : _config(config), _stats(stats) {}

IntegrationManager::~IntegrationManager() {
  stop();
}

bool IntegrationManager::init() {
  Logger::infoln(F("Initializing Integration Manager..."));

  registerIntegrations();

  bool anyInitialized = false;
  for (auto &integration : _integrations) {
    if (integration->init()) {
      Logger::infoln(F("Integration '%s' initialized successfully"), integration->getName());
      anyInitialized = true;
    } else {
      Logger::errorln(F("Failed to initialize integration '%s'"), integration->getName());
    }
  }

  if (anyInitialized) {
    Logger::infoln(F("Integration Manager initialized with %d active integrations"),
                   static_cast<int>(_integrations.size()));
  } else {
    Logger::infoln(F("Integration Manager initialized with no active integrations"));
  }

  return true; // Always return true as IntegrationManager itself initializes successfully
}

void IntegrationManager::process() {
  for (auto &integration : _integrations) {
    integration->process();
  }
}

void IntegrationManager::stop() {
  for (auto &integration : _integrations) {
    integration->stop();
  }
}

void IntegrationManager::updatePhoneState(AppState newState, AppState previousState) {
  for (auto &integration : _integrations) {
    integration->updatePhoneState(newState, previousState);
  }

  if (newState == AppState::IncomingCallRing) {
    updateRingState(true);
  } else if (previousState == AppState::IncomingCallRing) {
    updateRingState(false);
  }
}

void IntegrationManager::updateCallInfo(const String &number,
                                        bool isIncoming,
                                        unsigned long startTime) {
  for (auto &integration : _integrations) {
    integration->updateCallInfo(number, isIncoming, startTime);
  }
}

void IntegrationManager::updateDialingProgress(const String &currentNumber) {
  for (auto &integration : _integrations) {
    integration->updateDialingProgress(currentNumber);
  }
}

void IntegrationManager::updateRingState(bool isRinging) {
  for (auto &integration : _integrations) {
    integration->updateRingState(isRinging);
  }
}

void IntegrationManager::updateSystemStatus() {
  for (auto &integration : _integrations) {
    integration->updateSystemStatus();
  }
}

void IntegrationManager::setDialCallback(std::function<bool(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setDialCallback(callback);
  }
}

void IntegrationManager::setAnswerCallback(std::function<bool()> callback) {
  for (auto &integration : _integrations) {
    integration->setAnswerCallback(callback);
  }
}

void IntegrationManager::setHangupCallback(std::function<bool()> callback) {
  for (auto &integration : _integrations) {
    integration->setHangupCallback(callback);
  }
}

void IntegrationManager::setRingCallback(std::function<bool(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setRingCallback(callback);
  }
}

void IntegrationManager::setWebhookCallback(std::function<bool(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setWebhookCallback(callback);
  }
}

void IntegrationManager::setCallWaitingCallback(std::function<bool()> callback) {
  for (auto &integration : _integrations) {
    integration->setCallWaitingCallback(callback);
  }
}

void IntegrationManager::reportCallStart(const String &number, bool isIncoming) {
  for (auto &integration : _integrations) {
    integration->reportCallStart(number, isIncoming);
  }
}

void IntegrationManager::reportCallEnd(unsigned long duration) {
  for (auto &integration : _integrations) {
    integration->reportCallEnd(duration);
  }
}

void IntegrationManager::reportBlockedCall(const String &number) {
  for (auto &integration : _integrations) {
    integration->reportBlockedCall(number);
  }
}

void IntegrationManager::reportError(const String &error) {
  for (auto &integration : _integrations) {
    integration->reportError(error);
  }
}

void IntegrationManager::reportWebhookTrigger(const String &webhookId) {
  for (auto &integration : _integrations) {
    integration->reportWebhookTrigger(webhookId);
  }
}

void IntegrationManager::onConfigurationChanged() {
  for (auto &integration : _integrations) {
    integration->onConfigurationChanged();
  }
}

bool IntegrationManager::hasEnabledIntegrations() const {
  for (const auto &integration : _integrations) {
    if (integration->isEnabled()) {
      return true;
    }
  }
  return false;
}

void IntegrationManager::listIntegrations() const {
  Logger::infoln(F("Registered integrations:"));
  for (const auto &integration : _integrations) {
    Logger::infoln(F("  - %s: %s"),
                   integration->getName(),
                   integration->isEnabled() ? F("enabled") : F("disabled"));
  }
}

void IntegrationManager::registerIntegrations() {
  // Register available integrations based on compile-time flags

#ifdef HOME_ASSISTANT_INTEGRATION
  _integrations.push_back(std::unique_ptr<IIntegration>(new HAIntegration(_config, _stats)));
  Logger::infoln(F("Registered Home Assistant integration"));
#endif

  // Future integrations can be added here:
  // #ifdef ANDROID_INTEGRATION
  // _integrations.push_back(std::unique_ptr<IIntegration>(new AndroidIntegration(_config,
  // _stats))); Logger::infoln(F("Registered Android integration")); #endif

  // #ifdef MQTT_INTEGRATION
  // _integrations.push_back(std::unique_ptr<IIntegration>(new MqttIntegration(_config, _stats)));
  // Logger::infoln(F("Registered MQTT integration"));
  // #endif

  Logger::infoln(F("Registered %d integrations"), static_cast<int>(_integrations.size()));
}
