#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAIntegration.h"
#include "../IntegrationManager.h"  // For ConfigChangeEvent enum
#include "../../common/logger.h"
// #include "../../common/timeManager.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Helper function to get state name
const char *getStateName(AppState state) {
  switch (state) {
  case AppState::Startup:
    return "Startup";
  case AppState::CheckHardware:
    return "CheckHardware";
  case AppState::CheckLine:
    return "CheckLine";
  case AppState::Idle:
    return "Idle";
  case AppState::InvalidNumber:
    return "InvalidNumber";
  case AppState::IncomingCall:
    return "IncomingCall";
  case AppState::IncomingCallRing:
    return "IncomingCallRing";
  case AppState::InCall:
    return "InCall";
  case AppState::Dialing:
    return "Dialing";
  default:
    return "Unknown";
  }
}

HAIntegration::HAIntegration(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config), _stats(stats), _state(state), _webServer(config, stats, state) {}

bool HAIntegration::init() {
  Logger::infoln(F("Initializing Home Assistant integration..."));

  setupWebServerCallbacks();

  if (!_webServer.init()) {
    Logger::errorln(F("Failed to initialize HA web server"));
    return false;
  }

  // Broadcast initial state
  broadcastFullState();

  Logger::infoln(F("Home Assistant integration initialized successfully"));
  return true;
}

void HAIntegration::process() {
  _webServer.process();

  // Handle scheduled reset
  if (_resetRequested && millis() >= _resetScheduledTime) {
    Logger::infoln(F("HA: Executing scheduled device reset"));
    
    // Close WebSocket connections gracefully
    // Note: We need to access the WebSocket through the web server
    // The web server will handle its own cleanup
    _webServer.stop();
    
    // Additional delay to ensure cleanup
    delay(500);
    
    // Restart the device
    ESP.restart();
  }

  // Periodic updates
  unsigned long now = millis();

  // Update statistics periodically
  if (now - _lastStatsUpdate >= kStatsUpdateInterval) {
    _lastStatsUpdate = now;
    JsonDocument doc;
    JsonObject obj = createEventObject(doc, "system", "stats");
    addStatsInfo(obj);
    _webServer.broadcastStateUpdate(doc);
  }

  // Update system status periodically
  if (now - _lastSystemUpdate >= kSystemUpdateInterval) {
    _lastSystemUpdate = now;
    updateSystemStatus();
  }
}

void HAIntegration::stop() {
  _webServer.stop();
  Logger::infoln(F("Home Assistant integration stopped"));
}

void HAIntegration::updatePhoneState(AppState newState, AppState previousState) {
  Logger::infoln(F("HA: Phone state changed from %s to %s"),
                 getStateName(previousState),
                 getStateName(newState));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "state");
  obj["state"] = static_cast<int>(newState);
  obj["previousState"] = static_cast<int>(previousState);
  obj["stateName"] = getStateName(newState);

  addPhoneStateInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateCallInfo(const String &number, bool isIncoming, unsigned long startTime) {
  _currentCall.number = number;
  _currentCall.isIncoming = isIncoming;
  _currentCall.startTime = startTime > 0 ? startTime : millis();

  Logger::infoln(F("HA: Call info updated - %s call to/from %s"),
                 isIncoming ? F("Incoming") : F("Outgoing"),
                 number.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "call_info");
  addCallInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDialingProgress(const String &currentNumber) {
  if (_currentDialingNumber != currentNumber) {
    _currentDialingNumber = currentNumber;

    Logger::infoln(F("HA: Dialing progress - current number: %s"), currentNumber.c_str());

    JsonDocument doc;
    JsonObject obj = createEventObject(doc, "phone_state", "dialing");
    obj["currentNumber"] = currentNumber;
    _webServer.broadcastStateUpdate(doc);
  }
}

void HAIntegration::updateRingState(bool isRinging) {
  Logger::infoln(F("HA: Ring state changed - %s"), isRinging ? F("ringing") : F("not ringing"));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "ring");
  obj["isRinging"] = isRinging;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDndState(bool isDndActive) {
  Logger::infoln(F("HA: DND state updated - %s"), isDndActive ? F("active") : F("inactive"));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "dnd");
  obj["dndActive"] = isDndActive;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateSystemStatus() {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "status");
  addSystemInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) {
  _dialCallback = callback;
}

void HAIntegration::setAnswerCallback(std::function<IntegrationCallbackResult()> callback) {
  _answerCallback = callback;
}

void HAIntegration::setHangupCallback(std::function<IntegrationCallbackResult()> callback) {
  _hangupCallback = callback;
}

void HAIntegration::setRingCallback(std::function<IntegrationCallbackResult(const String &)> callback) {
  _ringCallback = callback;
}

void HAIntegration::setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) {
  _callWaitingCallback = callback;
}

void HAIntegration::setMaintenanceModeChangedCallback(std::function<void(bool)> callback) {
  _maintenanceModeChangedCallback = callback;
}

void HAIntegration::reportCallStart(const String &number, bool isIncoming) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to update call info and broadcast to HA
  updateCallInfo(number, isIncoming);

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "start");
  obj["number"] = number;
  obj["isIncoming"] = isIncoming;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportCallEnd(unsigned long duration) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to broadcast to HA
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "end");
  obj["duration"] = duration;
  _webServer.broadcastStateUpdate(doc);

  // Clear call info
  _currentCall.number = "";
  _currentCall.isIncoming = false;
  _currentCall.startTime = 0;
}

void HAIntegration::reportBlockedCall(const String &number) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to broadcast to HA
  Logger::infoln(F("HA: Blocked call from %s"), number.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "blocked");
  obj["number"] = number;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportError(const String &error) {
  Logger::errorln(F("HA: Error reported - %s"), error.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "error");
  obj["error"] = error;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::onConfigurationChanged() {
  Logger::infoln(F("HA: Configuration changed, broadcasting update"));
  broadcastFullState();
}

void HAIntegration::setupWebServerCallbacks() {
  _webServer.setStatusCallback([this](JsonObject &obj) { getFullStatus(obj); });

  _webServer.setStateUpdateCallback(
      [this](const String &command, const JsonVariant &data) -> HAOperationResult {
        Logger::infoln(F("HA: Received command: %s"), command.c_str());

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
        } else if (command == "quick_dial_add") {
          return handleAddQuickDial(data);
        } else if (command == "quick_dial_remove") {
          return handleRemoveQuickDial(data);
        } else if (command == "blocked_number_add") {
          return handleAddBlockedNumber(data);
        } else if (command == "blocked_number_remove") {
          return handleRemoveBlockedNumber(data);
        } else if (command == "webhook_add") {
          return handleAddWebhookAction(data);
        } else if (command == "webhook_remove") {
          return handleRemoveWebhookAction(data);
        } else if (command == "ha_url") {
          return handleSetHAUrl(data);
        } else if (command == "refetch_all") {
          return handleRefetchAll();
        } else if (command == "refresh") {
          broadcastFullState();
          return HAOperationResult(true);
        } else {
          Logger::warnln(F("HA: Unknown command: %s"), command.c_str());
          return HAOperationResult(false, "Unknown command: " + command);
        }
      });
}

void HAIntegration::broadcastFullState() {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "full_state", "");
  addBasicDeviceInfo(obj);
  addPhoneStateInfo(obj);
  addCallInfo(obj);
  addSystemInfo(obj);
  addStatsInfo(obj);

  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::broadcastStateChange(const String &key, const JsonVariant &value) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "config");
  obj[key] = value;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::addBasicDeviceInfo(JsonObject &obj) {
  obj["deviceId"] = _config.getDeviceId();
}

void HAIntegration::addPhoneStateInfo(JsonObject &obj) {
  obj["state"] = static_cast<int>(_state.newAppState);
  obj["stateName"] = getStateName(_state.newAppState);
  obj["dndActive"] = _state.isDnd;
  obj["maintenanceMode"] = _state.isMaintenanceMode;

  // Add call state information
  if (_state.callState.callNumber[0] != '\0') {
    obj["currentDialingNumber"] = _state.callState.callNumber;
  }
}

void HAIntegration::addCallInfo(JsonObject &obj) {
  if (!_currentCall.number.isEmpty()) {
    obj["currentCallNumber"] = _currentCall.number;
    obj["currentCallIsIncoming"] = _currentCall.isIncoming;
    obj["currentCallStartTime"] = _currentCall.startTime;

    if (_currentCall.startTime > 0) {
      obj["currentCallDuration"] = (millis() - _currentCall.startTime) / 1000;
    }
  }
}

void HAIntegration::addSystemInfo(JsonObject &obj) {
  obj["freeHeap"] = _stats.getFreeHeap();
  obj["rssi"] = _stats.getRSSI();
  obj["timestamp"] = millis(); // Use millis() instead of getUnixTime()
  obj["uptime"] = _stats.getUptime();
}

void HAIntegration::addStatsInfo(JsonObject &obj) {
  const CallStats &callStats = _stats.getCallStats();
  obj["totalCalls"] = callStats.totalCalls;
  obj["incomingCalls"] = callStats.incomingCalls;
  obj["outgoingCalls"] = callStats.outgoingCalls;
  obj["blockedCalls"] = callStats.blockedCalls;
  obj["totalTalkTimeSeconds"] = callStats.totalTalkTimeSeconds;
}

void HAIntegration::getFullStatus(JsonObject &obj) {
  // Status object with hierarchical structure
  JsonObject status = obj["status"].to<JsonObject>();

  // System status
  JsonObject systemStatus = status["system"].to<JsonObject>();
  systemStatus["uptime"] = _stats.getUptime();
  systemStatus["freeHeap"] = _stats.getFreeHeap();
  systemStatus["rssi"] = _stats.getRSSI();

  // Phone status
  JsonObject phoneStatus = status["phone"].to<JsonObject>();
  phoneStatus["maintenanceMode"] = _state.isMaintenanceMode;
  phoneStatus["state"] = static_cast<int>(_state.newAppState);
  phoneStatus["dndActive"] = _state.isDnd;
}

// Helper function to create WebSocket event objects
JsonObject
HAIntegration::createEventObject(JsonDocument &doc, const String &event, const String &type) {
  JsonObject obj = doc.to<JsonObject>();
  obj["event"] = event;
  obj["type"] = type;
  obj["timestamp"] = millis();
  return obj;
}

void HAIntegration::triggerWebhookHttp(const String &webhookId) {
  // Only trigger if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Logger::warnln(F("HA: Webhook trigger skipped - WiFi not connected"));
    return;
  }

  // Construct webhook URL
  String webhookUrl = _config.getHomeAssistantUrl() + "/api/webhook/" + webhookId;

  Logger::infoln(F("HA: Triggering webhook HTTP call: %s"), webhookUrl.c_str());

  HTTPClient http;
  http.begin(webhookUrl);
  http.setTimeout(5000); // 5 second timeout

  // Make POST request (webhooks typically expect POST)
  int httpResponseCode = http.POST("");

  if (httpResponseCode > 0) {
    Logger::infoln(F("HA: Webhook HTTP response: %d"), httpResponseCode);
    if (httpResponseCode == 200) {
      Logger::infoln(F("HA: Webhook triggered successfully"));
    }
  } else {
    Logger::errorln(F("HA: Webhook HTTP error: %s"), http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

void HAIntegration::triggerWebhook(const String &webhookId) {
  Logger::infoln(F("HA: Triggering webhook - %s"), webhookId.c_str());

  // Trigger the actual HTTP webhook call to Home Assistant
  triggerWebhookHttp(webhookId);
}

// Business logic methods for web server
HAOperationResult HAIntegration::handleDialRequest(const String &number) {
  if (!_dialCallback) {
    return HAOperationResult(false, "Dial callback not available");
  }

  if (number.isEmpty()) {
    return HAOperationResult(false, "Number cannot be empty");
  }

  IntegrationCallbackResult result = _dialCallback(number);
  if (result.success) {
    Logger::infoln(F("HA: Dial command successful for %s"), number.c_str());
    return HAOperationResult(true);
  } else {
    Logger::errorln(F("HA: Dial command failed for %s: %s"), number.c_str(), result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleAnswerRequest() {
  if (!_answerCallback) {
    return HAOperationResult(false, "Answer callback not available");
  }

  IntegrationCallbackResult result = _answerCallback();
  if (result.success) {
    Logger::infoln(F("HA: Answer command successful"));
    return HAOperationResult(true);
  } else {
    Logger::errorln(F("HA: Answer command failed: %s"), result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleHangupRequest() {
  if (!_hangupCallback) {
    return HAOperationResult(false, "Hangup callback not available");
  }

  IntegrationCallbackResult result = _hangupCallback();
  if (result.success) {
    Logger::infoln(F("HA: Hangup command successful"));
    return HAOperationResult(true);
  } else {
    Logger::errorln(F("HA: Hangup command failed: %s"), result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleSetDND(const JsonVariant &json) {
  DndConfig dndConfig = _config.getDndConfig();
  bool changed = false;

  if (json["force"].is<bool>()) {
    dndConfig.force = json["force"];
    changed = true;
  }

  if (json["scheduled"].is<bool>()) {
    dndConfig.scheduled = json["scheduled"];
    changed = true;
  }

  if (json["startHour"].is<int>()) {
    dndConfig.startHour = json["startHour"];
    changed = true;
  }

  if (json["startMinute"].is<int>()) {
    dndConfig.startMinute = json["startMinute"];
    changed = true;
  }

  if (json["endHour"].is<int>()) {
    dndConfig.endHour = json["endHour"];
    changed = true;
  }

  if (json["endMinute"].is<int>()) {
    dndConfig.endMinute = json["endMinute"];
    changed = true;
  }

  if (changed) {
    _config.setDndConfig(dndConfig);
    Logger::infoln(F("HA: DND configuration updated"));
    
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::DND_CONFIG_CHANGED);
    }
  }

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();
  data["dndActive"] = _state.isDnd;
  return result;
}

HAOperationResult HAIntegration::handleSetMaintenanceMode(const JsonVariant &json) {
  if (!json["enabled"].is<bool>()) {
    return HAOperationResult(false, "Missing 'enabled' parameter");
  }

  bool enabled = json["enabled"];
  _state.isMaintenanceMode = enabled;

  Logger::infoln(F("HA: Maintenance mode %s"), enabled ? F("enabled") : F("disabled"));

  if (_maintenanceModeChangedCallback) {
    _maintenanceModeChangedCallback(enabled);
  }

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();
  data["maintenanceMode"] = enabled;
  return result;
}

HAOperationResult HAIntegration::handleSetAudioConfig(const JsonVariant &json) {
  AudioConfig audioConfig = _config.getAudioConfig();
  bool changed = false;

  if (json["earpieceVolume"].is<int>()) {
    int volume = json["earpieceVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.earpieceVolume = volume;
      changed = true;
    } else {
      return HAOperationResult(false, "Earpiece volume must be between 1 and 7");
    }
  }

  if (json["earpieceGain"].is<int>()) {
    int gain = json["earpieceGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.earpieceGain = gain;
      changed = true;
    } else {
      return HAOperationResult(false, "Earpiece gain must be between 1 and 7");
    }
  }

  if (json["speakerVolume"].is<int>()) {
    int volume = json["speakerVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.speakerVolume = volume;
      changed = true;
    } else {
      return HAOperationResult(false, "Speaker volume must be between 1 and 7");
    }
  }

  if (json["speakerGain"].is<int>()) {
    int gain = json["speakerGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.speakerGain = gain;
      changed = true;
    } else {
      return HAOperationResult(false, "Speaker gain must be between 1 and 7");
    }
  }

  if (changed) {
    _config.setAudioConfig(audioConfig);
    Logger::infoln(F("HA: Audio configuration updated"));
    
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::AUDIO_CONFIG_CHANGED);
    }
  }

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();
  JsonObject config = data["config"].to<JsonObject>();
  JsonObject audio = config["audio"].to<JsonObject>();
  audio["earpieceVolume"] = audioConfig.earpieceVolume;
  audio["earpieceGain"] = audioConfig.earpieceGain;
  audio["speakerVolume"] = audioConfig.speakerVolume;
  audio["speakerGain"] = audioConfig.speakerGain;
  return result;
}

HAOperationResult HAIntegration::handleSetRingPattern(const JsonVariant &json) {
  String pattern = json["pattern"].as<String>();

  if (pattern.isEmpty()) {
    return HAOperationResult(false, "Ring pattern cannot be empty");
  }

  _config.setRingPattern(pattern);
  Logger::infoln(F("HA: Ring pattern set to: %s"), pattern.c_str());

  // Publish config change event
  if (_configChangeCallback) {
    _configChangeCallback(ConfigChangeEvent::RING_PATTERN_CHANGED);
  }

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();
  data["ringPattern"] = pattern;
  return result;
}

HAOperationResult HAIntegration::handleRingOperation(const JsonVariant &json) {
  if (!json["pattern"]) {
    return HAOperationResult(false, "Missing 'pattern' parameter");
  }

  String pattern = json["pattern"].as<String>();
  Logger::infoln(F("HA: Ring operation with pattern: %s"), pattern.c_str());

  if (!_ringCallback) {
    return HAOperationResult(false, "Ring callback not available");
  }

  IntegrationCallbackResult result = _ringCallback(pattern);
  if (result.success) {
    Logger::infoln(F("HA: Ring command successful with pattern %s"), pattern.c_str());
    return HAOperationResult(true);
  } else {
    Logger::errorln(F("HA: Ring command failed: %s"), result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleResetDevice() {
  Logger::infoln(F("HA: Device reset requested"));

  // Send graceful shutdown notification via WebSocket
  JsonDocument shutdownDoc;
  JsonObject shutdownObj = shutdownDoc.to<JsonObject>();
  shutdownObj["event"] = "system";
  shutdownObj["type"] = "shutdown";
  shutdownObj["timestamp"] = millis();
  shutdownObj["reason"] = "reset_requested";
  shutdownObj["message"] = "Device is shutting down for reset";

  _webServer.broadcastStateUpdate(shutdownDoc);

  // Return success immediately - the reset will happen after response is sent
  HAOperationResult result(true);
  
  // Use a simple timer approach to delay the reset
  // This allows the HTTP response to be sent first
  static unsigned long resetTime = millis() + 2500; // 2.5 seconds delay
  
  // Schedule the reset in the integration's process() method
  // by setting a flag that process() will check
  _resetRequested = true;
  _resetScheduledTime = resetTime;
  
  return result;
}

HAOperationResult HAIntegration::handleAddQuickDial(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"] || !jsonObj["number"]) {
    return HAOperationResult(false, "Missing required parameters: code, number");
  }

  String code = jsonObj["code"].as<String>();
  String number = jsonObj["number"].as<String>();
  String name = jsonObj["name"].as<String>(); // Optional

  if (code.isEmpty() || number.isEmpty()) {
    return HAOperationResult(false, "Code and number cannot be empty");
  }

  if (_config.hasQuickDialEntry(code) || _config.hasWebhookAction(code)) {
    return HAOperationResult(false, "Code already exists in quick dial or webhook actions");
  }

  if (_config.addQuickDialEntry(code, number, name)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::QUICK_DIAL_CHANGED);
    }
    
    HAOperationResult result(true);
    JsonObject data = result.data.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["code"] = code;
    entry["number"] = number;
    entry["name"] = name;
    return result;
  } else {
    return HAOperationResult(false, "Failed to add quick dial entry");
  }
}

HAOperationResult HAIntegration::handleRemoveQuickDial(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"]) {
    return HAOperationResult(false, "Missing required parameter: code");
  }

  String code = jsonObj["code"].as<String>();

  if (code.isEmpty()) {
    return HAOperationResult(false, "Code cannot be empty");
  }

  if (_config.removeQuickDialEntry(code)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::QUICK_DIAL_CHANGED);
    }
    
    return HAOperationResult(true);
  } else {
    return HAOperationResult(false, "Failed to remove quick dial entry or entry not found");
  }
}

HAOperationResult HAIntegration::handleDialQuickDial(const JsonVariant &json) {
  if (!json["code"]) {
    return HAOperationResult(false, "Missing 'code' parameter");
  }

  String code = json["code"].as<String>();

  // Look up the quick dial entry
  const auto &quickDialEntries = _config.getQuickDialEntries();
  auto it = std::find_if(quickDialEntries.begin(),
                         quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });

  if (it == quickDialEntries.end()) {
    return HAOperationResult(false, "Quick dial code not found");
  }

  String number = it->number;

  // Trigger the actual dial operation
  HAOperationResult dialResult = handleDialRequest(number);
  if (dialResult.success) {
    Logger::infoln(F("HA: Quick dial %s -> %s"), code.c_str(), number.c_str());
  }

  return dialResult;
}

HAOperationResult HAIntegration::handleToggleCallWaiting() {
  Logger::infoln(F("HA: Call waiting toggle requested"));

  if (!_callWaitingCallback) {
    return HAOperationResult(false, "Call waiting callback not available");
  }

  IntegrationCallbackResult result = _callWaitingCallback();
  if (result.success) {
    Logger::infoln(F("HA: Call waiting switch successful"));
    return HAOperationResult(true);
  } else {
    Logger::errorln(F("HA: Call waiting switch failed: %s"), result.errorMessage.c_str());
    return HAOperationResult(false, result.errorMessage);
  }
}

HAOperationResult HAIntegration::handleAddBlockedNumber(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["number"]) {
    return HAOperationResult(false, "Missing required parameter: number");
  }

  String number = jsonObj["number"].as<String>();
  String reason = jsonObj["reason"].as<String>(); // Optional

  if (number.isEmpty()) {
    return HAOperationResult(false, "Number cannot be empty");
  }

  if (_config.addBlockedNumber(number, reason)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::BLOCKED_NUMBER_CHANGED);
    }
    
    HAOperationResult result(true);
    JsonObject data = result.data.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["number"] = number;
    entry["reason"] = reason;
    return result;
  } else {
    return HAOperationResult(false, "Failed to add blocked number (may already exist)");
  }
}

HAOperationResult HAIntegration::handleRemoveBlockedNumber(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["number"]) {
    return HAOperationResult(false, "Missing required parameter: number");
  }

  String number = jsonObj["number"].as<String>();

  if (number.isEmpty()) {
    return HAOperationResult(false, "Number cannot be empty");
  }

  if (_config.removeBlockedNumber(number)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::BLOCKED_NUMBER_CHANGED);
    }
    
    HAOperationResult result(true);
    JsonObject data = result.data.to<JsonObject>();
    data["number"] = number;
    return result;
  } else {
    return HAOperationResult(false, "Blocked number not found");
  }
}

HAOperationResult HAIntegration::handleAddWebhookAction(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"] || !jsonObj["id"]) {
    return HAOperationResult(false, "Missing required parameters: code, id");
  }

  String code = jsonObj["code"].as<String>();
  String webhookId = jsonObj["id"].as<String>();
  String actionName = jsonObj["actionName"].as<String>(); // Optional

  if (code.isEmpty() || webhookId.isEmpty()) {
    return HAOperationResult(false, "Code and webhook ID cannot be empty");
  }

  if (_config.hasQuickDialEntry(code) || _config.hasWebhookAction(code)) {
    return HAOperationResult(false, "Code already exists in quick dial or webhook actions");
  }

  if (_config.addWebhookAction(code, webhookId, actionName)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::WEBHOOK_ACTION_CHANGED);
    }
    
    HAOperationResult result(true);
    JsonObject data = result.data.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["code"] = code;
    entry["id"] = webhookId;
    entry["actionName"] = actionName;
    return result;
  } else {
    return HAOperationResult(false, "Failed to add webhook action");
  }
}

HAOperationResult HAIntegration::handleRemoveWebhookAction(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"]) {
    return HAOperationResult(false, "Missing required parameter: code");
  }

  String code = jsonObj["code"].as<String>();

  if (code.isEmpty()) {
    return HAOperationResult(false, "Code cannot be empty");
  }

  if (_config.removeWebhookAction(code)) {
    // Publish config change event
    if (_configChangeCallback) {
      _configChangeCallback(ConfigChangeEvent::WEBHOOK_ACTION_CHANGED);
    }
    
    HAOperationResult result(true);
    JsonObject data = result.data.to<JsonObject>();
    data["code"] = code;
    return result;
  } else {
    return HAOperationResult(false, "Webhook action not found");
  }
}

HAOperationResult HAIntegration::handleSetHAUrl(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return HAOperationResult(false, "Invalid JSON");
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["url"]) {
    return HAOperationResult(false, "Missing required parameter: url");
  }

  String url = jsonObj["url"].as<String>();

  if (url.isEmpty()) {
    return HAOperationResult(false, "URL cannot be empty");
  }

  // Set the HA URL in device config for persistence
  _config.setHomeAssistantUrl(url);
  Logger::infoln(F("HA: Home Assistant URL set to: %s"), url.c_str());

  // Publish config change event
  if (_configChangeCallback) {
    _configChangeCallback(ConfigChangeEvent::HA_URL_CHANGED);
  }

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();
  data["url"] = url;
  return result;
}

HAOperationResult HAIntegration::handleRefetchAll() {
  Logger::infoln(F("HA: Refetch all data requested"));

  // Reload configuration from SPIFFS
  _config.load();
  _stats.load();

  HAOperationResult result(true);
  JsonObject data = result.data.to<JsonObject>();

  // Add basic device information to the result
  addBasicDeviceInfo(data);
  addPhoneStateInfo(data);
  addSystemInfo(data);
  addStatsInfo(data);

  return result;
}

void HAIntegration::setConfigChangeCallback(ConfigChangeCallback callback) {
  _configChangeCallback = callback;
  Logger::infoln(F("HA: Config change callback set"));
}

#endif // HOME_ASSISTANT_INTEGRATION
