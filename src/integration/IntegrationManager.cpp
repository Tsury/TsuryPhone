#include "IntegrationManager.h"
#include "../common/logger.h"

// Include available integrations
#ifdef HOME_ASSISTANT_INTEGRATION
#include "ha/HAIntegration.h"
#endif

IntegrationManager::IntegrationManager(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config),
      _stats(stats),
      _state(state),
      _statsManager(stats, state),
      _prevDndState(false),
      _prevMaintenanceMode(false),
      _prevAppState(AppState::Startup),
      _prevRingingState(false) {}

IntegrationManager::~IntegrationManager() {
  stop();
}

bool IntegrationManager::init() {
  Logger::infoln(F("Initializing Integration Manager..."));

  // Initialize StatsManager first
  if (!_statsManager.init()) {
    Logger::errorln(F("Failed to initialize StatsManager"));
    return false;
  }

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
  // Process StatsManager first for automatic stats tracking
  _statsManager.process();

  // Check for state changes
  checkForStateChanges();

  // Process all integrations
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

  // Note: Ring state changes are now handled automatically in checkForStateChanges()
}

void IntegrationManager::updateCallInfo(const String &number,
                                        bool isIncoming,
                                        unsigned long startTime) {
  // Notify StatsManager of call info change
  _statsManager.onCallInfoChanged(number, isIncoming);

  // Notify all integrations
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

void IntegrationManager::updateDndState(bool isDndActive) {
  for (auto &integration : _integrations) {
    integration->updateDndState(isDndActive);
  }
}

void IntegrationManager::setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setDialCallback(callback);
  }
}

void IntegrationManager::setAnswerCallback(std::function<IntegrationCallbackResult()> callback) {
  for (auto &integration : _integrations) {
    integration->setAnswerCallback(callback);
  }
}

void IntegrationManager::setHangupCallback(std::function<IntegrationCallbackResult()> callback) {
  for (auto &integration : _integrations) {
    integration->setHangupCallback(callback);
  }
}

void IntegrationManager::setRingCallback(std::function<IntegrationCallbackResult(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setRingCallback(callback);
  }
}

void IntegrationManager::setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) {
  for (auto &integration : _integrations) {
    integration->setCallWaitingCallback(callback);
  }
}

void IntegrationManager::setCallBlockedCallback(std::function<void(const String &)> callback) {
  _callBlockedCallback = callback;
}

void IntegrationManager::setMaintenanceModeChangedCallback(std::function<void(bool)> callback) {
  for (auto &integration : _integrations) {
    integration->setMaintenanceModeChangedCallback(callback);
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
  // Notify all integrations
  for (auto &integration : _integrations) {
    integration->reportBlockedCall(number);
  }
}

void IntegrationManager::reportError(const String &error) {
  for (auto &integration : _integrations) {
    integration->reportError(error);
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
  std::unique_ptr<HAIntegration> haIntegration(new HAIntegration(_config, _stats, _state));
  
  // Set up config change callback
  haIntegration->setConfigChangeCallback([this](ConfigChangeEvent event) {
    this->notifyConfigChange(event);
  });
  
  _integrations.push_back(std::move(haIntegration));
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

void IntegrationManager::checkForStateChanges() {
  // Check for phone state changes
  if (_state.newAppState != _prevAppState) {
    AppState currentState = _state.newAppState;

    // Notify StatsManager of state change for automatic stats tracking
    _statsManager.onPhoneStateChanged(currentState, _prevAppState);

    // Check for call start/end transitions
    bool currentCallActive = (currentState == AppState::InCall);
    bool prevCallActive = (_prevAppState == AppState::InCall);

    if (!prevCallActive && currentCallActive) {
      // Call started
      _callWasActive = true;
      _callStartTime = millis();

      // If we have call info, report call start
      if (_state.callState.callNumber[0] != '\0' &&
          strcmp(_state.callState.callNumber, _prevCallStateNumber.c_str()) != 0) {
        _prevCallStateNumber = String(_state.callState.callNumber);
        bool isIncoming =
            (currentState == AppState::InCall && (_prevAppState == AppState::IncomingCall ||
                                                  _prevAppState == AppState::IncomingCallRing));
        handleCallStarted(_prevCallStateNumber, isIncoming);
      }
    } else if (prevCallActive && !currentCallActive) {
      // Call ended
      if (_callWasActive) {
        unsigned long duration = (millis() - _callStartTime) / 1000;
        handleCallEnded(duration);
        _callWasActive = false;
        _callStartTime = 0;
        _prevCallStateNumber = ""; // Reset to allow same number to call again
      }
    }

    // Notify all integrations
    updatePhoneState(currentState, _prevAppState);
    _prevAppState = currentState;
  }

  // Check for call number changes (for automatic call info updates)
  if (strcmp(_state.callState.callNumber, _prevCallNumber.c_str()) != 0 &&
      _state.callState.callNumber[0] != '\0') {
    _prevCallNumber = String(_state.callState.callNumber);

    // Determine if it's incoming or outgoing based on state
    bool isIncoming = (_state.newAppState == AppState::IncomingCall ||
                       _state.newAppState == AppState::IncomingCallRing);

    // Check if incoming call should be blocked
    if (isIncoming && shouldBlockCall(_prevCallNumber)) {
      // Handle the blocked call automatically
      handleCallBlocked(_prevCallNumber);

      // Notify main.cpp via callback so it can take action (hangup, etc.)
      if (_callBlockedCallback) {
        _callBlockedCallback(_prevCallNumber);
      }
    } else {
      // Notify StatsManager and integrations
      _statsManager.onCallInfoChanged(_prevCallNumber, isIncoming);
      updateCallInfo(_prevCallNumber, isIncoming);

      // If we just got call info during an active call, report call start
      if (_callWasActive && _callStartTime > 0) {
        handleCallStarted(_prevCallNumber, isIncoming);
      }
    }
  }

  // Check for dialing progress changes
  if (strcmp(_state.currentDialingNumber, _prevDialingNumber.c_str()) != 0) {
    if (_state.currentDialingNumber[0] != '\0') {
      _prevDialingNumber = String(_state.currentDialingNumber);
      // Notify StatsManager and integrations
      _statsManager.onDialingProgressChanged(_prevDialingNumber);
      updateDialingProgress(_prevDialingNumber);
    } else {
      _prevDialingNumber = "";
    }
  }

  // Check for DND state changes
  if (_state.isDnd != _prevDndState) {
    _prevDndState = _state.isDnd;
    updateDndState(_state.isDnd);
  }

  // Check for maintenance mode changes
  bool currentMaintenanceMode = _state.isMaintenanceMode;
  if (currentMaintenanceMode != _prevMaintenanceMode) {
    _prevMaintenanceMode = currentMaintenanceMode;
    Logger::infoln(F("IntegrationManager: Maintenance mode changed to %s"),
                   currentMaintenanceMode ? "enabled" : "disabled");
    // Notify all integrations that system status has changed
    updatePhoneState(_state.newAppState, _state.prevAppState);
  }

  // Check for ring state changes (based on app state)
  bool currentRingingState = (_state.newAppState == AppState::IncomingCallRing);
  if (currentRingingState != _prevRingingState) {
    _prevRingingState = currentRingingState;
    updateRingState(currentRingingState);
  }

  // Could add more state change checks here if needed
  // For example, if we wanted to track other state changes automatically
}

void IntegrationManager::handleCallBlocked(const String &number) {
  Logger::infoln(F("IntegrationManager: Handling blocked call from %s"), number.c_str());

  // Notify StatsManager and integrations
  _statsManager.onCallBlocked(number);
  reportBlockedCall(number);
}

bool IntegrationManager::shouldBlockCall(const String &number) const {
  if (number.isEmpty()) {
    return false;
  }

  bool shouldBlock = _config.isIncomingCallBlocked(number);
  if (shouldBlock) {
    Logger::infoln(F("IntegrationManager: Call from %s should be blocked"), number.c_str());
  }

  return shouldBlock;
}

void IntegrationManager::updateMaintenanceMode(bool enabled) {
  Logger::infoln(F("IntegrationManager: Updating maintenance mode to %s"),
                 enabled ? "enabled" : "disabled");

  // Update the device state
  _state.isMaintenanceMode = enabled;
}

void IntegrationManager::handleCallStarted(const String &number, bool isIncoming) {
  Logger::infoln(F("IntegrationManager: Handling call start - %s call to/from %s"),
                 isIncoming ? F("Incoming") : F("Outgoing"),
                 number.c_str());

  // Notify integrations
  reportCallStart(number, isIncoming);
}

void IntegrationManager::handleCallEnded(unsigned long duration) {
  Logger::infoln(F("IntegrationManager: Handling call end - duration: %lu seconds"), duration);

  // Notify integrations
  reportCallEnd(duration);
}

void IntegrationManager::triggerWebhook(const String &webhookId) {
  for (auto &integration : _integrations) {
    if (integration->isEnabled()) {
      integration->triggerWebhook(webhookId);
    }
  }
}

// Config change event system implementation
void IntegrationManager::addConfigChangeCallback(ConfigChangeCallback callback) {
  _configChangeCallbacks.push_back(callback);
  Logger::infoln(F("Config change callback registered. Total callbacks: %d"), 
                 static_cast<int>(_configChangeCallbacks.size()));
}

void IntegrationManager::notifyConfigChange(ConfigChangeEvent event) {
  const char* eventName = "";
  switch (event) {
    case ConfigChangeEvent::DND_CONFIG_CHANGED: eventName = "DND_CONFIG_CHANGED"; break;
    case ConfigChangeEvent::AUDIO_CONFIG_CHANGED: eventName = "AUDIO_CONFIG_CHANGED"; break;
    case ConfigChangeEvent::QUICK_DIAL_CHANGED: eventName = "QUICK_DIAL_CHANGED"; break;
    case ConfigChangeEvent::BLOCKED_NUMBER_CHANGED: eventName = "BLOCKED_NUMBER_CHANGED"; break;
    case ConfigChangeEvent::WEBHOOK_ACTION_CHANGED: eventName = "WEBHOOK_ACTION_CHANGED"; break;
    case ConfigChangeEvent::HA_URL_CHANGED: eventName = "HA_URL_CHANGED"; break;
    case ConfigChangeEvent::RING_PATTERN_CHANGED: eventName = "RING_PATTERN_CHANGED"; break;
  }

  Logger::infoln(F("Notifying config change: %s to %d callbacks"), 
                 eventName, static_cast<int>(_configChangeCallbacks.size()));

  // Notify all registered callbacks
  for (const auto& callback : _configChangeCallbacks) {
    callback(event);
  }
}
