#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAIntegration.h"
#include "../../common/logger.h"
#include "../IntegrationLog.h"
#include "../IntegrationManager.h"
#include "../core/ConfigDiff.h"
#include <HTTPClient.h>
#include <WiFi.h>

HAIntegration::HAIntegration(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config),
      _stats(stats),
      _state(state),
      _webServer(config, stats, state),
      _integrationService(IntegrationService::shared(config, stats, state)),
      _haConfig(config),
      _haNumberHandler(_haConfig) {}

bool HAIntegration::init() {
  INTL_INFO("Initializing integration...");

  // Ensure shared service knows our integration tag for event root fields
  _integrationService.setIntegrationTag("ha");

  if (!_haConfig.init()) {
    Logger::errorln(F("Failed to initialize HA config"));
    return false;
  }

  setupWebServerCallbacks();

  if (!_webServer.init()) {
    Logger::errorln(F("Failed to initialize HA web server"));
    return false;
  }

  broadcastFullState();

  INTL_INFO("Initialized successfully");

  // Action handlers are registered via IntegrationManager::registerIntegrations() flow.
  return true;
}

void HAIntegration::registerActionHandlers(IntegrationManager &manager) {
  manager.addActionHandler(&_haNumberHandler);
}

void HAIntegration::process() {
  _webServer.process();

  if (_integrationService.processScheduledReset()) {
    stop();
  }

  unsigned long now = millis();
  if (now - _lastStatsUpdate >= kStatsUpdateInterval) {
    const uint32_t fp = _integrationService.getStatsFingerprint();

    if (fp != _lastStatsHash) {
      JsonDocument doc = _integrationService.buildSystemEvent("stats");
      _webServer.broadcastStateUpdate(doc);
      _lastStatsHash = fp;
      INTL_DEBUG("Stats broadcast (changed) fp=%lu", fp);
    } else {
      INTL_DEBUG("Stats suppressed (unchanged)");
    }

    _lastStatsUpdate = now;
  }

  if (now - _lastSystemUpdate >= kSystemUpdateInterval) {
    _lastSystemUpdate = now;
    updateSystemStatus();
  }
}

void HAIntegration::stop() {
  _webServer.stop();
  INTL_INFO("Stopped");
}

void HAIntegration::updatePhoneState(AppState newState, AppState previousState) {
  INTL_INFO("State %s -> %s", appStateToString(previousState), appStateToString(newState));

  JsonDocument doc = _integrationService.buildCurrentPhoneStateEvent("state");
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateCallInfo(const String &number, bool isIncoming, unsigned long startTime) {
  INTL_DEBUG("Call info %s %s", isIncoming ? "Incoming" : "Outgoing", number.c_str());

  JsonDocument doc = _integrationService.buildCurrentPhoneStateEvent("call_info");
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDialingProgress(const String &currentNumber) {
  INTL_DEBUG("Dialing %s", currentNumber.c_str());

  JsonDocument doc = _integrationService.buildCurrentPhoneStateEvent("dialing");
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateRingState(bool isRinging) {
  INTL_INFO("Ring %s", isRinging ? "on" : "off");

  JsonDocument doc = _integrationService.buildPhoneStateEvent("ring");
  JsonObject obj = doc.as<JsonObject>();
  obj["isRinging"] = isRinging;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDndState(bool isDndActive) {
  INTL_INFO("DND %s", isDndActive ? "active" : "inactive");

  JsonDocument doc = _integrationService.buildCurrentPhoneStateEvent("dnd");
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateSystemStatus() {
  JsonDocument doc = _integrationService.buildSystemEvent("status");
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::setDialCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
  _integrationService.setDialCallback(callback);
}

void HAIntegration::setAnswerCallback(std::function<IntegrationCallbackResult()> callback) {
  _integrationService.setAnswerCallback(callback);
}

void HAIntegration::setHangupCallback(std::function<IntegrationCallbackResult()> callback) {
  _integrationService.setHangupCallback(callback);
}

void HAIntegration::setRingCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
  _integrationService.setRingCallback(callback);
}

void HAIntegration::setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) {
  _integrationService.setCallWaitingCallback(callback);
}

void HAIntegration::setMaintenanceModeChangedCallback(std::function<void(bool)> callback) {
  _integrationService.setMaintenanceModeChangedCallback(callback);
}

void HAIntegration::setFactoryResetCallback(std::function<void()> callback) {
  _integrationService.setFactoryResetCallback(callback);
}
void HAIntegration::reportCallStart(const String &number, bool isIncoming) {
  updateCallInfo(number, isIncoming);
  // Preserve direction and omit fabricated start time in generic builder
  JsonDocument doc = _integrationService.buildCallEvent("start", number, isIncoming, 0);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportCallEnd(unsigned long duration) {
  // Use last known call number from state
  String number = String(_state.callState.callNumber);
  JsonDocument doc = _integrationService.buildCallEvent("end", number, false, duration);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportBlockedCall(const String &number) {
  INTL_WARN("Blocked call %s", number.c_str());
  JsonDocument doc = _integrationService.buildCallEvent("blocked", number, true, 0);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportError(const String &error) {
  INTL_ERROR("Error %s", error.c_str());

  JsonDocument doc = _integrationService.buildErrorEvent(error);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::onConfigurationChanged() {
  INTL_INFO("Config changed broadcast");
  broadcastFullState();
}

void HAIntegration::setupWebServerCallbacks() {
  _webServer.setStatusCallback([this](JsonObject &obj) { _integrationService.getFullStatus(obj); });

  _webServer.setStateUpdateCallback(
      [this](const String &command, const JsonVariant &data) -> HAOperationResult {
        INTL_DEBUG("Cmd %s", command.c_str());

        if (command == "dial") {
          return handleDialRequest(data["number"].as<String>());
        } else if (command == "answer") {
          return handleAnswerRequest();
        } else if (command == "hangup") {
          return handleHangupRequest();
        } else if (command == "dial_quick_dial") {
          return handleDialQuickDial(data);
        } else if (command == "switch_call_waiting") {
          return handleToggleCallWaiting();
        } else if (command == "dnd") {
          return handleSetDND(data);
        } else if (command == "maintenance_mode") {
          return handleSetMaintenanceMode(data);
        } else if (command == "audio_config") {
          return handleSetAudioConfig(data);
        } else if (command == "ring_pattern") {
          return handleSetRingPattern(data);
        } else if (command == "ring") {
          return handleRingOperation(data);
        } else if (command == "reset") {
          return handleResetDevice();
        } else if (command == "factory_reset") {
          return handleFactoryReset();
        } else if (command == "quick_dial_add") {
          return handleAddQuickDial(data);
        } else if (command == "quick_dial_remove") {
          return handleRemoveQuickDial(data);
        } else if (command == "blocked_add") {
          return handleAddBlockedNumber(data);
        } else if (command == "blocked_remove") {
          return handleRemoveBlockedNumber(data);
        } else if (command == "priority_add") {
          return handleAddPriorityCaller(data);
        } else if (command == "priority_remove") {
          return handleRemovePriorityCaller(data);
        } else if (command == "webhook_add") {
          return handleAddWebhookAction(data);
        } else if (command == "webhook_remove") {
          return handleRemoveWebhookAction(data);
        } else if (command == "ha_url") {
          HAOperationResult r = handleSetHAUrl(data);
          if (r.success && data["url"]) {
            broadcastStateChange("ha.url", data["url"].as<String>());
          }
          return r;
        } else if (command == "tsuryphone_config") {
          return handleGetTsuryPhoneConfig();
        } else if (command == "refetch_all") {
          return handleRefetchAll();
        } else if (command == "refresh") {
          broadcastFullState();
          return HAOperationResult(true);
        } else {
          INTL_WARN("Unknown cmd %s", command.c_str());
          return HAOperationResult(false, "Unknown command: " + command);
        }
      });
}

void HAIntegration::broadcastFullState() {
  JsonDocument doc = _integrationService.buildCurrentFullStateEvent();

  // Add HA-specific data (webhook actions)
  if (doc["phone"]) {
    JsonObject phone = doc["phone"];
    JsonArray webhooks = phone["webhooks"].to<JsonArray>();
    for (const auto &entry : _haConfig.getWebhookActions()) {
      JsonObject entryObj = webhooks.add<JsonObject>();
      entryObj["code"] = entry.code;
      entryObj["id"] = entry.id;
      entryObj["actionName"] = entry.actionName;
    }

    // Add HA URL
    phone["homeAssistantUrl"] = _haConfig.getHomeAssistantUrl();
  }

  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::broadcastStateChange(const String &key, const JsonVariant &value) {
  // Old value not tracked for simple key notifications
  JsonDocument doc = _integrationService.buildSystemConfigEvent(key, value, JsonVariantConst());
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::broadcastStateChange(const String &key, const String &value) {
  JsonDocument tmp;
  tmp.set(value);
  broadcastStateChange(key, tmp.as<JsonVariant>());
}

void HAIntegration::broadcastStateChange(const String &key, bool value) {
  JsonDocument tmp;
  tmp.set(value);
  broadcastStateChange(key, tmp.as<JsonVariant>());
}

void HAIntegration::broadcastStateChange(const String &key, int value) {
  JsonDocument tmp;
  tmp.set(value);
  broadcastStateChange(key, tmp.as<JsonVariant>());
}

void HAIntegration::triggerAction(const String &actionId) {
  // In HA context, actionId is a webhook id
  INTL_INFO("Trigger action %s", actionId.c_str());

  // Only trigger if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    INTL_WARN("Action skipped (WiFi down)");
    return;
  }

  String webhookUrl = _haConfig.getHomeAssistantUrl() + "/api/webhook/" + actionId;
  HTTPClient http;
  http.begin(webhookUrl);
  http.setTimeout(5000);
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0) {
    INTL_DEBUG("Webhook HTTP %d", httpResponseCode);
  } else {
    INTL_ERROR("Webhook HTTP err %s", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

HAOperationResult HAIntegration::convertResult(const IntegrationCallbackResult &result) {
  HAOperationResult haResult(result.success, result.errorMessage);
  haResult.errorCode = result.errorCode;
  if (result.success && !result.data.isNull()) {
    haResult.data = result.data;
  }
  return haResult;
}

HAOperationResult HAIntegration::handleDialRequest(const String &number) {
  IntegrationCallbackResult result = _integrationService.handleDialRequest(number);
  if (result.success) {
    INTL_INFO("Dial success %s", number.c_str());
  }
  return convertResult(result);
}

HAOperationResult HAIntegration::handleAnswerRequest() {
  IntegrationCallbackResult result = _integrationService.handleAnswerRequest();
  if (result.success) {
    INTL_INFO("Answer success");
  }
  return convertResult(result);
}

HAOperationResult HAIntegration::handleHangupRequest() {
  IntegrationCallbackResult result = _integrationService.handleHangupRequest();
  if (result.success) {
    INTL_INFO("Hangup success");
  }
  return convertResult(result);
}

HAOperationResult HAIntegration::handleSetDND(const JsonVariant &json) {
  // Snapshot previous config
  DndConfig prev = _config.getDndConfig();
  IntegrationCallbackResult result = _integrationService.handleSetDND(json);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    DndConfig curr = _config.getDndConfig();
    JsonDocument agg;
    if (buildDndConfigDelta(_integrationService, prev, curr, agg)) {
      _webServer.broadcastStateUpdate(agg);
    }
  }
  return haResult;
}

HAOperationResult HAIntegration::handleSetMaintenanceMode(const JsonVariant &json) {
  bool enabled = json["enabled"];
  IntegrationCallbackResult result = _integrationService.handleSetMaintenanceMode(enabled);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    broadcastStateChange("maintenance.enabled", enabled);
  }
  return haResult;
}

HAOperationResult HAIntegration::handleSetAudioConfig(const JsonVariant &json) {
  AudioConfig before = _config.getAudioConfig();
  IntegrationCallbackResult result = _integrationService.handleSetAudioConfig(json);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    AudioConfig after = _config.getAudioConfig();
    JsonDocument agg;
    if (buildAudioConfigDelta(_integrationService, before, after, agg)) {
      _webServer.broadcastStateUpdate(agg);
    }
  }
  return haResult;
}

HAOperationResult HAIntegration::handleSetRingPattern(const JsonVariant &json) {
  String pattern = json["pattern"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleSetRingPattern(pattern);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    broadcastStateChange("ring.pattern", pattern);
  }
  return haResult;
}

HAOperationResult HAIntegration::handleRingOperation(const JsonVariant &json) {
  String pattern = "";
  if (json["pattern"]) {
    pattern = json["pattern"].as<String>();
  }

  INTL_INFO("Ring op pattern %s", pattern.isEmpty() ? "[default]" : pattern.c_str());

  IntegrationCallbackResult result = _integrationService.handleRingOperation(pattern);
  if (result.success) {
    INTL_INFO("Ring success pattern %s", pattern.isEmpty() ? "[default]" : pattern.c_str());
    return HAOperationResult(true);
  } else {
    INTL_ERROR("Ring failed %s", result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleResetDevice() {
  IntegrationCallbackResult result = _integrationService.handleResetDevice();
  if (result.success) {
    INTL_INFO("Device reset scheduled");
    JsonDocument shutdownDoc = _integrationService.buildShutdownEvent("reset_requested");
    _webServer.broadcastStateUpdate(shutdownDoc);
  }
  return convertResult(result);
}

HAOperationResult HAIntegration::handleFactoryReset() {
  IntegrationCallbackResult result = _integrationService.handleFactoryReset();
  if (result.success) {
    INTL_WARN("Factory reset scheduled");
    JsonDocument shutdownDoc =
        _integrationService.buildShutdownEvent("factory_reset_requested");
    _webServer.broadcastStateUpdate(shutdownDoc);
  }
  return convertResult(result);
}

HAOperationResult HAIntegration::handleAddQuickDial(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();

  String code = jsonObj["code"].as<String>();
  String number = jsonObj["number"].as<String>();
  String name = jsonObj["name"].as<String>(); // Optional

  IntegrationCallbackResult result = _integrationService.handleAddQuickDial(code, number, name);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    // Broadcast structured payload for future extensibility
    JsonDocument payload;
    JsonObject obj = payload.to<JsonObject>();
    obj["code"] = code;
    obj["number"] = number;
    if (!name.isEmpty()) {
      obj["name"] = name;
    }
    // Wrap object to JsonVariant for correct overload resolution
    broadcastStateChange("quick_dial.add", payload.as<JsonVariant>());
  }
  return haResult;
}

HAOperationResult HAIntegration::handleRemoveQuickDial(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String code = jsonObj["code"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleRemoveQuickDial(code);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    broadcastStateChange("quick_dial.remove", code);
  }
  return haResult;
}

HAOperationResult HAIntegration::handleDialQuickDial(const JsonVariant &json) {
  String code = json["code"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleDialQuickDial(code);

  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    INTL_INFO("Quick dial success");
  }

  return haResult;
}

HAOperationResult HAIntegration::handleToggleCallWaiting() {
  INTL_INFO("Call waiting toggle req");

  IntegrationCallbackResult result = _integrationService.handleToggleCallWaiting();
  if (result.success) {
    INTL_INFO("Call waiting switch success");
    return HAOperationResult(true);
  } else {
    INTL_ERROR("Call waiting switch failed %s", result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleAddBlockedNumber(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();

  String number = jsonObj["number"].as<String>();
  String reason = jsonObj["reason"].as<String>(); // Optional

  IntegrationCallbackResult result = _integrationService.handleAddBlockedNumber(number, reason);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    JsonDocument payload;
    JsonObject obj = payload.to<JsonObject>();
    obj["number"] = number;
    if (!reason.isEmpty()) {
      obj["reason"] = reason;
    }
    broadcastStateChange("blocked.add", payload.as<JsonVariant>());
  }
  return haResult;
}

HAOperationResult HAIntegration::handleAddPriorityCaller(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String number = jsonObj["number"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleAddPriorityCaller(number);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    JsonDocument payload;
    JsonObject obj = payload.to<JsonObject>();
    obj["number"] = number;
    broadcastStateChange("priority.add", payload.as<JsonVariant>());
  }
  return haResult;
}

HAOperationResult HAIntegration::handleRemovePriorityCaller(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String number = jsonObj["number"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleRemovePriorityCaller(number);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    broadcastStateChange("priority.remove", number);
  }
  return haResult;
}

HAOperationResult HAIntegration::handleRemoveBlockedNumber(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String number = jsonObj["number"].as<String>();

  IntegrationCallbackResult result = _integrationService.handleRemoveBlockedNumber(number);
  HAOperationResult haResult = convertResult(result);
  if (haResult.success) {
    broadcastStateChange("blocked.remove", number);
  }
  return haResult;
}

HAOperationResult HAIntegration::handleAddWebhookAction(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();

  String code = jsonObj["code"].as<String>();
  String webhookId = jsonObj["id"].as<String>();
  String actionName = jsonObj["actionName"].as<String>(); // Optional

  if (code.isEmpty() || webhookId.isEmpty()) {
    return HAOperationResult(false, "Code and webhook ID cannot be empty");
  }

  if (_haConfig.isCodeConflict(code)) {
    return HAOperationResult(false, "Code already exists in quick dial or webhook actions");
  }

  if (_haConfig.addWebhookAction(code, webhookId, actionName)) {
    JsonDocument resultData;
    JsonObject data = resultData.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["code"] = code;
    entry["id"] = webhookId;
    entry["actionName"] = actionName;

    // Emit structured config delta for webhook addition
    JsonDocument payload;
    JsonObject w = payload.to<JsonObject>();
    w["code"] = code;
    w["id"] = webhookId;
    if (!actionName.isEmpty()) {
      w["actionName"] = actionName;
    }
    broadcastStateChange("webhook.add", payload.as<JsonVariant>());

    HAOperationResult result(true);
    result.data = resultData;
    return result;
  }
  return HAOperationResult(false, "Failed to add webhook action");
}

HAOperationResult HAIntegration::handleRemoveWebhookAction(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String code = jsonObj["code"].as<String>();

  if (code.isEmpty()) {
    return HAOperationResult(false, "Code cannot be empty");
  }

  if (_haConfig.removeWebhookAction(code)) {
    broadcastStateChange("webhook.remove", code);
    return HAOperationResult(true);
  }
  return HAOperationResult(false, "Webhook action not found");
}

HAOperationResult HAIntegration::handleSetHAUrl(const JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  String url = jsonObj["url"].as<String>();

  if (url.isEmpty()) {
    return HAOperationResult(false, "URL cannot be empty");
  }

  _haConfig.setHomeAssistantUrl(url);

  // Broadcast state change
  broadcastFullState(); // Use full state broadcast to include updated URL

  return HAOperationResult(true);
}

HAOperationResult HAIntegration::handleGetTsuryPhoneConfig() {
  INTL_INFO("Get config req");

  // Build config data including HA-specific webhook actions
  JsonDocument configData = _integrationService.buildDeviceConfig();

  // Add HA-specific data
  if (configData["phone"]) {
    JsonObject phone = configData["phone"];
    JsonArray webhooks = phone["webhooks"].to<JsonArray>();
    for (const auto &entry : _haConfig.getWebhookActions()) {
      JsonObject entryObj = webhooks.add<JsonObject>();
      entryObj["code"] = entry.code;
      entryObj["id"] = entry.id;
      entryObj["actionName"] = entry.actionName;
    }

    // Add HA URL
    phone["homeAssistantUrl"] = _haConfig.getHomeAssistantUrl();
  }

  HAOperationResult haResult(true);
  haResult.data = configData;

  return haResult;
}

HAOperationResult HAIntegration::handleRefetchAll() {
  INTL_INFO("Refetch all req");

  IntegrationCallbackResult result = _integrationService.handleRefetchAll();

  HAOperationResult haResult = convertResult(result);
  if (result.success) {
    haResult.data = _integrationService.buildAllData();
  }

  return haResult;
}

#endif
