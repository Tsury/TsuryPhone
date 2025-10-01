#if defined(HOME_ASSISTANT_INTEGRATION) || defined(ANDROID_INTEGRATION)

#include "IntegrationManager.h"
#include "../common/logger.h"
#include "IntegrationLog.h"
#include <map>

#include "IntegrationService.h"

// Forward declarations to avoid header conflicts
class TsuryPhone;
struct IntegrationCallbackResult;

// Include available integrations
#ifdef HOME_ASSISTANT_INTEGRATION
#include "ha/HAIntegration.h"
#endif
#ifdef MOCK_INTEGRATION
#include "mock/MockIntegration.h"
#endif

IntegrationManager::IntegrationManager(TsuryPhone &tsuryPhone, DeviceConfig &config, State &state)
    : _tsuryPhone(tsuryPhone),
      _config(config),
      _stats(),
      _state(state),
      _statsManager(_stats, state),
      _prevDndState(false),
      _prevMaintenanceMode(false),
  _prevHookOff(false),
      _prevAppState(AppState::Startup),
      _prevRingingState(false) {}

IntegrationManager::~IntegrationManager() {
  stop();
}

bool IntegrationManager::init() {
  INT_LOG_INFO("CORE", "Initializing Integration Manager...");

  // Initialize DeviceStats first
  if (!_stats.init()) {
    Logger::errorln(F("Failed to initialize DeviceStats"));
    return false;
  }

  // Initialize StatsManager
  if (!_statsManager.init()) {
    Logger::errorln(F("Failed to initialize StatsManager"));
    return false;
  }

  registerIntegrations();

  bool anyInitialized = false;
  for (auto &integration : _integrations) {
    if (integration->init()) {
      INT_LOG_INFO("CORE", "Integration '%s' initialized successfully", integration->getTag());
      anyInitialized = true;
    } else {
      INT_LOG_ERROR("CORE", "Failed to initialize integration '%s'", integration->getTag());
    }
  }

  if (anyInitialized) {
    INT_LOG_INFO("CORE",
                 "Integration Manager initialized with %d active integrations",
                 static_cast<int>(_integrations.size()));
  } else {
    INT_LOG_WARN("CORE", "Integration Manager initialized with no active integrations");
  }

  return true; // Always return true as IntegrationManager itself initializes successfully
}

void IntegrationManager::process() {
  // Process StatsManager first for automatic stats tracking
  _statsManager.process();

  // Check for state changes
  checkForStateChanges();

  // Handle any queued debug serial characters (m/j/k) centrally
  if (!_debugCharQueue.empty()) {
    for (char c : _debugCharQueue) {
      switch (c) {
      case 'm':
        exportMetricsSnapshot();
        break;
      case 'j':
        emitStructuredJsonLog("debug_test", "manual");
        break;
      case 'k':
        emitStructuredJsonLogKV("kv", "mode", _state.isMaintenanceMode ? "maintenance" : "normal");
        break;
      default:
        break; // Ignore other chars
      }
    }
    _debugCharQueue.clear();
  }

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

void IntegrationManager::onFactoryResetInitiated() {
  emitStructuredJsonLog("factory_reset", "initiated");
}

void IntegrationManager::onFactoryResetBeforeRestart() {
  _stats.reset();
  emitStructuredJsonLog("factory_reset", "restarting");
}

void IntegrationManager::setDialCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
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

void IntegrationManager::setRingCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
  for (auto &integration : _integrations) {
    integration->setRingCallback(callback);
  }
}

void IntegrationManager::setCallWaitingCallback(
    std::function<IntegrationCallbackResult()> callback) {
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

void IntegrationManager::setFactoryResetCallback(std::function<void()> callback) {
  for (auto &integration : _integrations) {
    integration->setFactoryResetCallback(callback);
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
  INT_LOG_INFO("CORE", "Registered integrations:");
  for (const auto &integration : _integrations) {
    INT_LOG_INFO("CORE",
                 "  - %s: %s",
                 integration->getTag(),
                 integration->isEnabled() ? "enabled" : "disabled");
  }
}

void IntegrationManager::registerIntegrations() {
  // Register available integrations based on compile-time flags

#ifdef HOME_ASSISTANT_INTEGRATION
  std::unique_ptr<HAIntegration> haIntegration(new HAIntegration(_config, _stats, _state));

  // Register HA number handler as action handler if capability present
  // Access underlying handler via dynamic_cast to concrete type (acceptable here during
  // registration) This cast will be removed once a generic integration-provided registration hook
  // exists.
  _integrations.push_back(std::move(haIntegration));
  INT_LOG_INFO("CORE", "Registered Home Assistant integration");
#endif

#ifdef MOCK_INTEGRATION
  std::unique_ptr<MockIntegration> mockInt(new MockIntegration());
  _integrations.push_back(std::move(mockInt));
  INT_LOG_INFO("CORE", "Registered Mock integration");
#endif

  // Future integrations can be added here:
  // #ifdef ANDROID_INTEGRATION
  // _integrations.push_back(std::unique_ptr<IIntegration>(new AndroidIntegration(_config,
  // _stats))); Logger::infoln(F("Registered Android integration")); #endif

  // #ifdef MQTT_INTEGRATION
  // _integrations.push_back(std::unique_ptr<IIntegration>(new MqttIntegration(_config, _stats)));
  // Logger::infoln(F("Registered MQTT integration"));
  // #endif

  INT_LOG_INFO("CORE", "Registered %d integrations", static_cast<int>(_integrations.size()));

  // Log capability summary
  for (const auto &integration : _integrations) {
    uint32_t caps = integration->getCapabilities();
    INT_LOG_INFO("CORE",
                 "Capabilities for %s: %s%s",
                 integration->getTag(),
                 (caps & IIntegration::IC_ACTIONS) ? "ACTIONS " : "",
                 (caps & IIntegration::IC_NUMBER_CODE_HANDLER) ? "NUMBER_CODES" : "");
    // Let integration register its action handlers if any
    integration->registerActionHandlers(*this);
  }

  // Conflict detection (M3): enumerate all handler codes and detect duplicates
  if (!_actionHandlers.empty()) {
    std::map<String, int> counts;
    for (auto *h : _actionHandlers) {
      for (const auto &code : h->listFullCodes()) {
        counts[code]++;
      }
    }
    for (const auto &kv : counts) {
      if (kv.second > 1) {
        INT_LOG_WARN("CORE",
                     "Action code conflict '%s' claimed by %d handlers",
                     kv.first.c_str(),
                     kv.second);
      }
    }

    // Emit structured JSON summary of action codes (T4.3)
    String json = F("{\"type\":\"action_codes\",\"integration\":\"core\",\"ts\":");
    json += String(millis());
    json += F(",\"codes\":[");
    bool first = true;
    for (auto &kv : counts) {
      if (!first) {
        json += ',';
      } else {
        first = false;
      }
      json += F("{\"code\":\"");
      json += kv.first;
      json += F("\",\"count\":");
      json += String(kv.second);
      json += '}';
    }
    json += ']';
    json += '}';
    Logger::infoln(json.c_str());
  }
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
      auto &svc = IntegrationService::shared(_config, _stats, _state);
      svc.setCurrentCallStartTs(_callStartTime);

      // If we have call info, report call start
      if (_state.callState.callNumber[0] != '\0' &&
          strcmp(_state.callState.callNumber, _prevCallStateNumber.c_str()) != 0) {
        _prevCallStateNumber = String(_state.callState.callNumber);
        bool isIncoming =
            (currentState == AppState::InCall && (_prevAppState == AppState::IncomingCall ||
                                                  _prevAppState == AppState::IncomingCallRing));
        auto &svc = IntegrationService::shared(_config, _stats, _state);
        svc.setCurrentCallDirection(isIncoming);
        handleCallStarted(_prevCallStateNumber, isIncoming);
      }
    } else if (prevCallActive && !currentCallActive) {
      // Call ended
      if (_callWasActive) {
        unsigned long duration = (millis() - _callStartTime) / 1000;
        handleCallEnded(duration);
        auto &svc = IntegrationService::shared(_config, _stats, _state);
        svc.clearCurrentCallStartTs();
        svc.setCurrentCallDirection(false);
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

    // Classify number (blocked / priority) once here; no hidden side-effects.
    auto cls = _config.classifyNumber(_prevCallNumber);
    // Set priority flag (non-blocking paths). Blocked takes precedence if both somehow true.
    _state.callState.isPriority = (!cls.isBlocked && cls.isPriority);

    if (isIncoming && cls.isBlocked) {
      handleCallBlocked(_prevCallNumber);
      IntegrationService::shared(_config, _stats, _state).setCurrentCallDirection(true);
      if (_callBlockedCallback) {
        _callBlockedCallback(_prevCallNumber);
      }
    } else {
      // Notify StatsManager and integrations
      _statsManager.onCallInfoChanged(_prevCallNumber, isIncoming);
      updateCallInfo(_prevCallNumber, isIncoming);

      // If we just got call info during an active call, report call start
      if (_callWasActive && _callStartTime > 0) {
        auto &svc = IntegrationService::shared(_config, _stats, _state);
        svc.setCurrentCallDirection(isIncoming);
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

  if (_state.isHookOff != _prevHookOff) {
    _prevHookOff = _state.isHookOff;
    INT_LOG_INFO("CORE", "Hook state changed: %s", _state.isHookOff ? "off" : "on");
    updatePhoneState(_state.newAppState, _prevAppState);
  }

  // Check for maintenance mode changes
  bool currentMaintenanceMode = _state.isMaintenanceMode;
  if (currentMaintenanceMode != _prevMaintenanceMode) {
    _prevMaintenanceMode = currentMaintenanceMode;
    INT_LOG_INFO(
        "CORE", "Maintenance mode changed to %s", currentMaintenanceMode ? "enabled" : "disabled");
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
  INT_LOG_WARN("CORE", "Blocked call from %s", number.c_str());

  // Notify StatsManager and integrations
  _statsManager.onCallBlocked(number);
  reportBlockedCall(number);
}

void IntegrationManager::updateMaintenanceMode(bool enabled) {
  INT_LOG_INFO("CORE", "Updating maintenance mode to %s", enabled ? "enabled" : "disabled");

  // Update the device state
  _state.isMaintenanceMode = enabled;
}

void IntegrationManager::handleCallStarted(const String &number, bool isIncoming) {
  INT_LOG_INFO("CORE",
               "Call start - %s call to/from %s",
               isIncoming ? "Incoming" : "Outgoing",
               number.c_str());

  // Notify integrations
  reportCallStart(number, isIncoming);
}

void IntegrationManager::handleCallEnded(unsigned long duration) {
  INT_LOG_INFO("CORE", "Call end - duration: %lu seconds", duration);

  // Notify integrations
  reportCallEnd(duration);
}

void IntegrationManager::triggerAction(const String &actionId) {
  INT_LOG_INFO("CORE", "Trigger action %s", actionId.c_str());
  for (auto &integration : _integrations) {
    if (!integration->isEnabled()) {
      continue;
    }
    uint32_t caps = integration->getCapabilities();
    if (caps & IIntegration::IC_ACTIONS) {
      integration->triggerAction(actionId);
    } else {
      INT_LOG_DEBUG("CORE", "Integration %s lacks ACTIONS capability", integration->getTag());
    }
  }
}

void IntegrationManager::enableIntegrationDebugLogging(bool enabled) {
  setIntegrationDebugLogging(enabled);
  INT_LOG_INFO("CORE", "Integration debug logging %s", enabled ? "ENABLED" : "DISABLED");
}

void IntegrationManager::enqueueDebugChar(char c) {
  // Only queue recognized debug commands to keep memory bounded
  if (c == 'm' || c == 'j' || c == 'k') {
    _debugCharQueue.push_back(c);
  }
}

void IntegrationManager::addActionHandler(IIntegrationActionHandler *handler) {
  if (!handler) {
    return;
  }
  _actionHandlers.push_back(handler);
  INT_LOG_INFO(
      "CORE", "Registered action handler (total=%d)", static_cast<int>(_actionHandlers.size()));
}

bool IntegrationManager::hasPartialActionMatch(const String &dialed) const {
  for (auto *h : _actionHandlers) {
    if (h->isPartialMatch(dialed)) {
      return true;
    }
  }
  return false;
}

bool IntegrationManager::isActionCode(const String &dialed) const {
  for (auto *h : _actionHandlers) {
    if (h->isFullMatch(dialed)) {
      return true;
    }
  }
  return false;
}

String IntegrationManager::resolveActionId(const String &dialed) const {
  for (auto *h : _actionHandlers) {
    if (h->isFullMatch(dialed)) {
      return h->resolveActionId(dialed);
    }
  }
  return String();
}

void IntegrationManager::listActionCodes() const {
  INT_LOG_INFO(
      "CORE", "Listing action codes (%d handlers)", static_cast<int>(_actionHandlers.size()));
  for (auto *h : _actionHandlers) {
    auto codes = h->listFullCodes();
    for (const auto &c : codes) {
      INT_LOG_INFO("CORE", "  code=%s", c.c_str());
    }
  }
}

void IntegrationManager::exportMetricsSnapshot() const {
  const CallStats &cs = _stats.getCallStats();
  // Neutral dotted key style (T5.3)
  INT_LOG_INFO("CORE",
               "METRICS calls.total=%lu calls.in=%lu calls.out=%lu calls.blocked=%lu "
               "calls.talkTimeSeconds=%lu system.uptimeMs=%lu system.heapFree=%lu system.rssi=%d",
               (unsigned long)cs.totalCalls,
               (unsigned long)cs.incomingCalls,
               (unsigned long)cs.outgoingCalls,
               (unsigned long)cs.blockedCalls,
               (unsigned long)cs.totalTalkTimeSeconds,
               (unsigned long)_stats.getUptime(),
               (unsigned long)_stats.getFreeHeap(),
               _stats.getRSSI());
}

void IntegrationManager::emitStructuredJsonLog(const char *event, const char *detail) const {
  // Minimal JSON (avoid dynamic allocation) - using Arduino String for convenience
  String json = F("{\"type\":\"integration_event\",\"seq\":");
  json += String(_jsonLogSeq++);
  json += F(",\"ts\":");
  json += String(millis());
  json += F(",\"event\":\"");
  json += event;
  json += F("\",\"detail\":\"");
  json += detail;
  json += F("\",\"integration\":\"core\"}");
  Logger::infoln(json.c_str());
}

void IntegrationManager::emitStructuredJsonLogKV(const char *event,
                                                 const char *k,
                                                 const char *v) const {
  String json = F("{\"type\":\"integration_event\",\"seq\":");
  json += String(_jsonLogSeq++);
  json += F(",\"ts\":");
  json += String(millis());
  json += F(",\"event\":\"");
  json += event;
  json += F("\",\"data\":{\"");
  json += k;
  json += F("\":\"");
  json += v;
  json += F("\"},\"integration\":\"core\"}");
  Logger::infoln(json.c_str());
}

// Config change event system implementation
void IntegrationManager::addConfigChangeCallback(ConfigChangeCallback callback) {
  _configChangeCallbacks.push_back(callback);
  INT_LOG_DEBUG("CORE",
                "Config change callback registered. Total callbacks: %d",
                static_cast<int>(_configChangeCallbacks.size()));
}

void IntegrationManager::notifyConfigChange(ConfigChangeEvent event) {
  const char *name = (event == ConfigChangeEvent::DND_CONFIG_CHANGED)     ? "DND_CONFIG_CHANGED"
                     : (event == ConfigChangeEvent::AUDIO_CONFIG_CHANGED) ? "AUDIO_CONFIG_CHANGED"
                     : (event == ConfigChangeEvent::QUICK_DIAL_CHANGED)   ? "QUICK_DIAL_CHANGED"
                     : (event == ConfigChangeEvent::BLOCKED_NUMBER_CHANGED)
                         ? "BLOCKED_NUMBER_CHANGED"
                     : (event == ConfigChangeEvent::RING_PATTERN_CHANGED) ? "RING_PATTERN_CHANGED"
                     : (event == ConfigChangeEvent::INTEGRATION_EXTENSION_CHANGED)
                         ? "INTEGRATION_EXTENSION_CHANGED"
                         : "UNKNOWN_CONFIG_EVENT";

  INT_LOG_DEBUG("CORE",
                "Config change %s (%d callbacks)",
                name,
                static_cast<int>(_configChangeCallbacks.size()));

  // Notify all registered callbacks
  for (const auto &callback : _configChangeCallbacks) {
    callback(event);
  }
}

void IntegrationManager::setupTsuryPhoneCallbacks(
    std::function<IntegrationCallbackResult(const String &)> dialCallback,
    std::function<IntegrationCallbackResult()> answerCallback,
    std::function<IntegrationCallbackResult()> hangupCallback,
    std::function<IntegrationCallbackResult(const String &)> ringCallback,
    std::function<IntegrationCallbackResult()> callWaitingCallback,
    std::function<void(const String &)> callBlockedCallback,
    std::function<void(bool)> maintenanceModeCallback,
  std::function<void()> factoryResetCallback,
    std::function<void(ConfigChangeEvent)> configChangeCallback) {

  INT_LOG_INFO("CORE", "Setting up callbacks");

  // Set up device operation callbacks for all integrations
  setDialCallback(dialCallback);
  setAnswerCallback(answerCallback);
  setHangupCallback(hangupCallback);
  setRingCallback(ringCallback);
  setCallWaitingCallback(callWaitingCallback);
  setCallBlockedCallback(callBlockedCallback);
  setMaintenanceModeChangedCallback(maintenanceModeCallback);
  setFactoryResetCallback(factoryResetCallback);
  addConfigChangeCallback(configChangeCallback);
}

// Webhook-specific helper methods removed (replaced by future generic action handler registry)

#endif // HOME_ASSISTANT_INTEGRATION || ANDROID_INTEGRATION