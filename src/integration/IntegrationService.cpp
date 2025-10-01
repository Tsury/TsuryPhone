#if defined(HOME_ASSISTANT_INTEGRATION) || defined(ANDROID_INTEGRATION)

#include "IntegrationService.h"
#include "../common/logger.h"
#include "IntegrationLog.h"
#include <algorithm>

IntegrationService::IntegrationService(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config), _stats(stats), _state(state) {}

IntegrationService &
IntegrationService::shared(DeviceConfig &config, DeviceStats &stats, State &state) {
  static IntegrationService instance(config, stats, state);
  return instance;
}

uint32_t IntegrationService::getStatsFingerprint() const {
  // Maintenance-free change detector: return DeviceStats revision.
  // Any update path that calls save() will bump the revision and trigger a broadcast.
  return _stats.getRevision();
}

void IntegrationService::setDialCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
  _dialCallback = callback;
}

void IntegrationService::setAnswerCallback(std::function<IntegrationCallbackResult()> callback) {
  _answerCallback = callback;
}

void IntegrationService::setHangupCallback(std::function<IntegrationCallbackResult()> callback) {
  _hangupCallback = callback;
}

void IntegrationService::setRingCallback(
    std::function<IntegrationCallbackResult(const String &)> callback) {
  _ringCallback = callback;
}

void IntegrationService::setCallWaitingCallback(
    std::function<IntegrationCallbackResult()> callback) {
  _callWaitingCallback = callback;
}

void IntegrationService::setMaintenanceModeChangedCallback(std::function<void(bool)> callback) {
  _maintenanceModeChangedCallback = callback;
}

void IntegrationService::setFactoryResetCallback(std::function<void()> callback) {
  _factoryResetCallback = callback;
}

IntegrationCallbackResult IntegrationService::handleDialRequest(const String &number) {
  if (!_dialCallback) {
    return IntegrationCallbackResult(false, "Dial callback not available");
  }

  if (number.isEmpty()) {
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  IntegrationCallbackResult result = _dialCallback(number);
  if (result.success) {
    INT_LOG_INFO("CORE", "Dial success %s", number.c_str());
  } else {
    INT_LOG_ERROR("CORE", "Dial failed %s : %s", number.c_str(), result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleAnswerRequest() {
  if (!_answerCallback) {
    return IntegrationCallbackResult(false, "Answer callback not available");
  }

  IntegrationCallbackResult result = _answerCallback();
  if (result.success) {
    INT_LOG_INFO("CORE", "Answer success");
  } else {
    INT_LOG_ERROR("CORE", "Answer failed %s", result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleHangupRequest() {
  if (!_hangupCallback) {
    return IntegrationCallbackResult(false, "Hangup callback not available");
  }

  IntegrationCallbackResult result = _hangupCallback();
  if (result.success) {
    INT_LOG_INFO("CORE", "Hangup success");
  } else {
    INT_LOG_ERROR("CORE", "Hangup failed %s", result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleDialQuickDial(const String &code) {
  if (code.isEmpty()) {
    return IntegrationCallbackResult(false, "Missing 'code' parameter");
  }

  // Look up the quick dial entry
  const auto &quickDialEntries = _config.getQuickDialEntries();
  auto it = std::find_if(quickDialEntries.begin(),
                         quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });

  if (it == quickDialEntries.end()) {
    return IntegrationCallbackResult(false, "Quick dial code not found");
  }

  String number = it->number;

  // Trigger the actual dial operation
  IntegrationCallbackResult dialResult = handleDialRequest(number);
  if (dialResult.success) {
    INT_LOG_INFO("CORE", "QuickDial %s -> %s", code.c_str(), number.c_str());
  }

  return dialResult;
}

IntegrationCallbackResult IntegrationService::handleToggleCallWaiting() {
  INT_LOG_INFO("CORE", "Call waiting toggle request");

  if (!_callWaitingCallback) {
    return IntegrationCallbackResult(false, "Call waiting callback not available");
  }

  IntegrationCallbackResult result = _callWaitingCallback();
  if (result.success) {
    INT_LOG_INFO("CORE", "Call waiting switch success");
  } else {
    INT_LOG_ERROR("CORE", "Call waiting switch failed %s", result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleRingOperation(const String &pattern) {
  INT_LOG_INFO("CORE", "Ring operation pattern %s", pattern.c_str());

  if (!_ringCallback) {
    return IntegrationCallbackResult(false, "Ring callback not available");
  }

  IntegrationCallbackResult result = _ringCallback(pattern);
  if (result.success) {
    INT_LOG_INFO("CORE", "Ring success pattern %s", pattern.c_str());
  } else {
    INT_LOG_ERROR("CORE", "Ring failed %s", result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleSetDND(const JsonVariant &json) {
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
    INT_LOG_INFO("CORE", "DND config updated");

    // Config change event will be emitted externally by higher-level component (C2)
  }

  // Create response data
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  data["dndActive"] = _state.isDnd;

  return IntegrationCallbackResult(true, resultData);
}

IntegrationCallbackResult IntegrationService::handleSetMaintenanceMode(bool enabled) {
  _state.isMaintenanceMode = enabled;

  INT_LOG_INFO("CORE", "Maintenance mode %s", enabled ? "enabled" : "disabled");

  if (_maintenanceModeChangedCallback) {
    _maintenanceModeChangedCallback(enabled);
  }

  // Create response data
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  data["isMaintenanceMode"] = enabled; // DS9 boolean normalization

  return IntegrationCallbackResult(true, resultData);
}

IntegrationCallbackResult IntegrationService::handleSetAudioConfig(const JsonVariant &json) {
  AudioConfig audioConfig = _config.getAudioConfig();
  bool changed = false;

  if (json["earpieceVolume"].is<int>()) {
    int volume = json["earpieceVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.earpieceVolume = volume;
      changed = true;
    } else {
      return IntegrationCallbackResult(false, "Earpiece volume must be between 1 and 7");
    }
  }

  if (json["earpieceGain"].is<int>()) {
    int gain = json["earpieceGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.earpieceGain = gain;
      changed = true;
    } else {
      return IntegrationCallbackResult(false, "Earpiece gain must be between 1 and 7");
    }
  }

  if (json["speakerVolume"].is<int>()) {
    int volume = json["speakerVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.speakerVolume = volume;
      changed = true;
    } else {
      return IntegrationCallbackResult(false, "Speaker volume must be between 1 and 7");
    }
  }

  if (json["speakerGain"].is<int>()) {
    int gain = json["speakerGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.speakerGain = gain;
      changed = true;
    } else {
      return IntegrationCallbackResult(false, "Speaker gain must be between 1 and 7");
    }
  }

  if (changed) {
    _config.setAudioConfig(audioConfig);
    INT_LOG_INFO("CORE", "Audio config updated");

    // Publish config change event
    // Config change event emitted externally (C2)
  }

  // Create response data
  AudioConfig currentAudioConfig = _config.getAudioConfig();
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  JsonObject config = data["config"].to<JsonObject>();
  JsonObject audio = config["audio"].to<JsonObject>();
  audio["earpieceVolume"] = currentAudioConfig.earpieceVolume;
  audio["earpieceGain"] = currentAudioConfig.earpieceGain;
  audio["speakerVolume"] = currentAudioConfig.speakerVolume;
  audio["speakerGain"] = currentAudioConfig.speakerGain;

  return IntegrationCallbackResult(true, resultData);
}

IntegrationCallbackResult IntegrationService::handleSetRingPattern(const String &pattern) {
  if (pattern.isEmpty()) {
    return IntegrationCallbackResult(false, "Ring pattern cannot be empty");
  }

  _config.setRingPattern(pattern);
  INT_LOG_INFO("CORE", "Ring pattern %s", pattern.c_str());

  // Publish config change event
  // Config change event emitted externally (C2)

  // Create response data (normalized under config.ring.pattern per DS7)
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  JsonObject config = data["config"].to<JsonObject>();
  JsonObject ring = config["ring"].to<JsonObject>();
  ring["pattern"] = pattern;

  return IntegrationCallbackResult(true, resultData);
}

IntegrationCallbackResult IntegrationService::handleAddQuickDial(const String &code,
                                                                 const String &number,
                                                                 const String &name) {
  if (code.isEmpty() || number.isEmpty()) {
    return IntegrationCallbackResult(false, "Code and number cannot be empty");
  }

  if (_config.hasQuickDialEntry(code)) {
    return IntegrationCallbackResult(false, "Code already exists in quick dial entries");
  }

  if (_config.addQuickDialEntry(code, number, name)) {
    // Publish config change event
    // Config change event emitted externally (C2)

    // Create response data
    JsonDocument resultData;
    JsonObject data = resultData.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["code"] = code;
    entry["number"] = number;
    entry["name"] = name;

    return IntegrationCallbackResult(true, resultData);
  } else {
    return IntegrationCallbackResult(false, "Failed to add quick dial entry");
  }
}

IntegrationCallbackResult IntegrationService::handleRemoveQuickDial(const String &code) {
  if (code.isEmpty()) {
    return IntegrationCallbackResult(false, "Code cannot be empty");
  }

  if (_config.removeQuickDialEntry(code)) {
    // Publish config change event
    // Config change event emitted externally (C2)

    return IntegrationCallbackResult(true);
  } else {
    return IntegrationCallbackResult(false, "Failed to remove quick dial entry or entry not found");
  }
}

IntegrationCallbackResult IntegrationService::handleAddBlockedNumber(const String &number,
                                                                     const String &reason) {
  if (number.isEmpty()) {
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  // Explicit conflict: cannot block a number that is currently a priority caller
  if (_config.isPriorityCaller(number)) {
    INT_LOG_WARN("CORE", "Blocked add rejected: %s is a priority caller", number.c_str());
    return IntegrationCallbackResult(false, "Number is a priority caller");
  }

  if (_config.addBlockedNumber(number, reason)) {
    // Publish config change event
    // Config change event emitted externally (C2)

    // Create response data
    JsonDocument resultData;
    JsonObject data = resultData.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["number"] = number;
    entry["reason"] = reason;

    return IntegrationCallbackResult(true, resultData);
  } else {
    return IntegrationCallbackResult(false, "Failed to add blocked number (exists or invalid)");
  }
}

IntegrationCallbackResult IntegrationService::handleRemoveBlockedNumber(const String &number) {
  if (number.isEmpty()) {
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  if (_config.removeBlockedNumber(number)) {
    // Publish config change event
    // Config change event emitted externally (C2)

    return IntegrationCallbackResult(true);
  } else {
    return IntegrationCallbackResult(false, "Blocked number not found");
  }
}

IntegrationCallbackResult IntegrationService::handleAddPriorityCaller(const String &number) {
  if (number.isEmpty()) {
    INT_LOG_WARN("CORE", "Priority add rejected: empty number");
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  if (_config.isIncomingCallBlocked(number)) {
    INT_LOG_WARN("CORE", "Priority add rejected: %s is currently blocked", number.c_str());
    return IntegrationCallbackResult(false, "Number is blocked");
  }

  if (!_config.addPriorityCaller(number)) {
    INT_LOG_WARN("CORE", "Priority add failed (exists or conflict) %s", number.c_str());
    return IntegrationCallbackResult(false, "Failed to add priority caller (already exists?)");
  }

  INT_LOG_INFO("CORE",
               "Priority caller added %s total=%u",
               number.c_str(),
               (unsigned)_config.getPriorityCallers().size());

  // Create response data
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  JsonObject entry = data["entry"].to<JsonObject>();
  entry["number"] = number;
  entry["priority"] = true;
  return IntegrationCallbackResult(true, resultData);
}

IntegrationCallbackResult IntegrationService::handleRemovePriorityCaller(const String &number) {
  if (number.isEmpty()) {
    INT_LOG_WARN("CORE", "Priority remove rejected: empty number");
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  if (!_config.removePriorityCaller(number)) {
    INT_LOG_WARN("CORE", "Priority remove failed (not found) %s", number.c_str());
    return IntegrationCallbackResult(false, "Priority caller not found");
  }

  INT_LOG_INFO("CORE",
               "Priority caller removed %s remaining=%u",
               number.c_str(),
               (unsigned)_config.getPriorityCallers().size());

  return IntegrationCallbackResult(true);
}

IntegrationCallbackResult IntegrationService::handleGetTsuryPhoneConfig() {
  INT_LOG_INFO("CORE", "Get config request");
  return IntegrationCallbackResult(true);
}

IntegrationCallbackResult IntegrationService::handleRefetchAll() {
  INT_LOG_INFO("CORE", "Refetch all data");

  // Reload configuration from SPIFFS
  _config.load();
  _stats.load();

  return IntegrationCallbackResult(true);
}

IntegrationCallbackResult IntegrationService::handleResetDevice() {
  INT_LOG_INFO("CORE", "Device reset requested");

  // Schedule the reset with a delay to allow response to be sent
  _resetRequested = true;
  _factoryResetRequested = false;
  _resetScheduledTime = millis() + 2500; // 2.5 seconds delay

  return IntegrationCallbackResult(true);
}

IntegrationCallbackResult IntegrationService::handleFactoryReset() {
  INT_LOG_WARN("CORE", "Factory reset requested");

  if (!_factoryResetCallback) {
    INT_LOG_ERROR("CORE", "Factory reset callback not available");
    return IntegrationCallbackResult(false, "Factory reset not supported");
  }

  _resetRequested = true;
  _factoryResetRequested = true;
  _resetScheduledTime = millis() + 2500; // match standard reset delay for response flush

  return IntegrationCallbackResult(true);
}

bool IntegrationService::processScheduledReset() {
  if (_resetRequested && millis() >= _resetScheduledTime) {
    INT_LOG_WARN("CORE", "Executing scheduled device %s",
                 _factoryResetRequested ? "factory reset" : "reset");

    // Additional delay to ensure cleanup
    delay(500);

    if (_factoryResetRequested && _factoryResetCallback) {
      _factoryResetCallback();
    } else {
      ESP.restart();
    }

    return true; // This line won't be reached, but for completeness
  }
  return false;
}

// Webhook HTTP trigger logic removed from generic layer; now integration-specific.

// JSON serialization methods for integration clients
JsonDocument IntegrationService::buildAllData() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();

  addStatus(data);
  addConfig(data);
  addStats(data);
  addPhone(data);

  return doc;
}

JsonDocument IntegrationService::buildSystemStatus() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();
  addStatus(data);
  return doc;
}

JsonDocument IntegrationService::buildPhoneConfig() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();
  addConfig(data);
  return doc;
}

JsonDocument IntegrationService::buildCallStats() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();
  addStats(data);
  return doc;
}

JsonDocument IntegrationService::buildPhoneData() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();
  addPhone(data);
  return doc;
}

JsonDocument IntegrationService::buildDeviceConfig() {
  JsonDocument doc;
  JsonObject data = doc.to<JsonObject>();
  data["deviceId"] = _config.getDeviceId();
  addConfig(data);
  addPhone(data);
  return doc;
}

// Individual JSON builders (moved from HAWebServer)
void IntegrationService::addStatus(JsonObject &doc) {
  JsonObject status = doc["status"].to<JsonObject>();

  // System status - reuse addSystemInfo logic
  JsonObject systemStatus = status["system"].to<JsonObject>();
  addSystemInfo(systemStatus);

  // Phone status - use basic phone status without extra fields
  JsonObject phoneStatus = status["phone"].to<JsonObject>();
  addBasicPhoneStatus(phoneStatus);
}

void IntegrationService::addConfig(JsonObject &doc) {
  JsonObject config = doc["config"].to<JsonObject>();

  // Audio config
  const AudioConfig &audioConfig = _config.getAudioConfig();
  JsonObject audio = config["audio"].to<JsonObject>();
  audio["earpieceVolume"] = audioConfig.earpieceVolume;
  audio["earpieceGain"] = audioConfig.earpieceGain;
  audio["speakerVolume"] = audioConfig.speakerVolume;
  audio["speakerGain"] = audioConfig.speakerGain;

  // DND config
  const DndConfig &dndConfig = _config.getDndConfig();
  JsonObject dnd = config["dnd"].to<JsonObject>();
  dnd["force"] = dndConfig.force;
  dnd["schedule"] = dndConfig.scheduled;
  dnd["startMinute"] = dndConfig.startMinute;
  dnd["endMinute"] = dndConfig.endMinute;
  dnd["startHour"] = dndConfig.startHour;
  dnd["endHour"] = dndConfig.endHour;

  // Ring config (DS7 normalized)
  JsonObject ring = config["ring"].to<JsonObject>();
  ring["pattern"] = _config.getRingPattern();
}

void IntegrationService::addStats(JsonObject &doc) {
  const CallStats &callStats = _stats.getCallStats();
  JsonObject stats = doc["stats"].to<JsonObject>();

  JsonObject calls = stats["calls"].to<JsonObject>();
  JsonObject totals = calls["totals"].to<JsonObject>();
  addStatsInfo(totals); // Reuse the logic instead of duplicating

  JsonObject lastCall = calls["lastCall"].to<JsonObject>();
  lastCall["number"] = callStats.lastCall.number;
  lastCall["type"] = callStats.lastCall.type;

  JsonObject systemStats = stats["system"].to<JsonObject>();
  systemStats["resets"] = _stats.getResetCount();
}

void IntegrationService::addPhone(JsonObject &doc) {
  JsonObject phone = doc["phone"].to<JsonObject>();

  // Include current phone state snapshot alongside configuration lists (HA parity)
  addPhoneStateInfo(phone);

  // Quick dial entries
  JsonArray quickDial = phone["quickDial"].to<JsonArray>();
  for (const auto &entry : _config.getQuickDialEntries()) {
    JsonObject entryObj = quickDial.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
  }

  // Blocked numbers
  JsonArray blocked = phone["blocked"].to<JsonArray>();
  for (const auto &entry : _config.getBlockedNumbers()) {
    JsonObject entryObj = blocked.add<JsonObject>();
    entryObj["number"] = entry.number;
    entryObj["reason"] = entry.reason;
  }

  // Priority callers
  JsonArray priority = phone["priorityCallers"].to<JsonArray>();
  for (const auto &num : _config.getPriorityCallers()) {
    priority.add(num);
  }
}

// Data builders for events and broadcasts (integration-agnostic)
void IntegrationService::addBasicDeviceInfo(JsonObject &obj) {
  obj["deviceId"] = _config.getDeviceId();
}

void IntegrationService::addPhoneStateInfo(JsonObject &obj) {
  obj["state"] = static_cast<int>(_state.newAppState);
  obj["stateName"] = appStateToString(_state.newAppState);
  obj["previousState"] = static_cast<int>(_state.prevAppState);
  obj["previousStateName"] = appStateToString(_state.prevAppState);
  obj["dndActive"] = _state.isDnd;
  obj["isMaintenanceMode"] = _state.isMaintenanceMode; // DS9 boolean normalization
  obj["isHookOff"] = _state.isHookOff;

  // Add active call number if present
  if (_state.callState.callNumber[0] != '\0') {
    obj["currentCallNumber"] = _state.callState.callNumber;
    obj["currentCallIsPriority"] = _state.callState.isPriority;
  }
  // Add dialing buffer number if present (distinct from active call)
  if (_state.currentDialingNumber[0] != '\0') {
    obj["currentDialingNumber"] = _state.currentDialingNumber;
  }
}

void IntegrationService::addBasicPhoneStatus(JsonObject &obj) {
  // Reuse addPhoneStateInfo to eliminate duplication
  addPhoneStateInfo(obj);
}

void IntegrationService::addCallInfo(JsonObject &obj,
                                     const String &callNumber,
                                     bool isIncoming,
                                     unsigned long startTime) {
  if (!callNumber.isEmpty()) {
    obj["currentCallNumber"] = callNumber;
    obj["isIncomingCall"] = isIncoming;
    obj["callStartTs"] = startTime; // ms
    if (startTime > 0) {
      obj["currentCallDurationMs"] = (uint32_t)(millis() - startTime);
    }
  }
}

void IntegrationService::addSystemInfo(JsonObject &obj) {
  obj["freeHeap"] = _stats.getFreeHeap();
  obj["rssi"] = _stats.getRSSI();
  obj["uptime"] = _stats.getUptime();
}

void IntegrationService::addStatsInfo(JsonObject &obj) {
  const CallStats &callStats = _stats.getCallStats();
  // DS8: Nested stats structure
  JsonObject calls = obj["calls"].to<JsonObject>();
  JsonObject totals = calls["totals"].to<JsonObject>();
  totals["total"] = callStats.totalCalls;
  totals["incoming"] = callStats.incomingCalls;
  totals["outgoing"] = callStats.outgoingCalls;
  totals["blocked"] = callStats.blockedCalls;
  totals["talkTimeSeconds"] = callStats.totalTalkTimeSeconds;
}

JsonObject IntegrationService::createEventObject(JsonDocument &doc,
                                                 const String &category,
                                                 const String &event) {
  JsonObject obj = doc.to<JsonObject>();
  obj["schemaVersion"] = INTEGRATION_EVENT_SCHEMA_VERSION;
  obj["category"] = category;
  obj["event"] = event;
  obj["ts"] = millis();
  obj["seq"] = nextSeq();
  obj["integration"] = _integrationTag; // set by concrete integration wrapper
  addBasicDeviceInfo(obj);
  return obj;
}

// DS4 unified response helpers
JsonDocument IntegrationService::buildSuccessResponse(const JsonVariantConst &data) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["schemaVersion"] = INTEGRATION_EVENT_SCHEMA_VERSION;
  root["success"] = true;
  root["ts"] = millis();
  if (!data.isNull()) {
    root["data"] = data;
  }
  return doc;
}

JsonDocument IntegrationService::buildErrorResponse(const String &message) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["schemaVersion"] = INTEGRATION_EVENT_SCHEMA_VERSION;
  root["success"] = false;
  root["ts"] = millis();
  root["message"] = message; // errorCode to be added DS5
  return doc;
}

// Overload with error code
JsonDocument IntegrationService::buildErrorResponse(const String &message, const String &code) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["schemaVersion"] = INTEGRATION_EVENT_SCHEMA_VERSION;
  root["success"] = false;
  root["ts"] = millis();
  root["errorCode"] = code;
  root["message"] = message;
  return doc;
}

void IntegrationService::getFullStatus(JsonObject &obj) {
  // Use the existing addStatus method for consistency
  addStatus(obj);
}

// Event-specific builders
JsonDocument IntegrationService::buildShutdownEvent(const String &reason) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "shutdown");
  obj["reason"] = reason;
  obj["message"] = "Device is shutting down for " + reason;
  return doc;
}

JsonDocument IntegrationService::buildErrorEvent(const String &error) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "error");
  obj["error"] = error;
  return doc;
}

JsonDocument IntegrationService::buildCallEvent(const String &eventType,
                                                const String &number,
                                                bool isIncoming,
                                                unsigned long duration) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", eventType);
  if (!number.isEmpty()) {
    obj["number"] = number;
  }
  // Use stored direction if available (override parameter if internal state set)
  bool effectiveIncoming = _currentCallIsIncoming ? true : isIncoming;
  if (eventType == "start") {
    obj["isIncoming"] = effectiveIncoming;
    obj["callStartTs"] = _currentCallStartTs > 0 ? _currentCallStartTs : millis();
  } else if (eventType == "end") {
    obj["isIncoming"] = effectiveIncoming;
    obj["callStartTs"] = _currentCallStartTs;
    if (duration > 0) {
      obj["durationMs"] = (uint32_t)(duration * 1000UL);
    }
  } else if (eventType == "blocked") {
    obj["isIncoming"] = true; // blocked implies incoming
  }
  return doc;
}

JsonDocument IntegrationService::buildPhoneStateEvent(const String &eventType,
                                                      AppState newState,
                                                      AppState previousState,
                                                      const String &currentNumber) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", eventType);

  if (eventType == "state") {
    obj["state"] = static_cast<int>(newState);
    obj["previousState"] = static_cast<int>(previousState);
    obj["stateName"] = appStateToString(newState);
    addPhoneStateInfo(obj);
  } else if (eventType == "call_info") {
    // Snapshot only (no fabricated start time here)
    addCallInfo(obj, currentNumber, false, 0);
  } else if (eventType == "dialing") {
    obj["currentDialingNumber"] = currentNumber;
  } else if (eventType == "ring") {
    obj["isRinging"] = true; // Assuming true when creating ring event
  } else if (eventType == "dnd") {
    obj["dndActive"] = _state.isDnd;
  }

  return doc;
}

// Convenience method that uses current state from _state object
JsonDocument IntegrationService::buildCurrentPhoneStateEvent(const String &eventType) {
  // Extract current state values and delegate to the main method
  if (eventType == "state") {
    return buildPhoneStateEvent(eventType, _state.newAppState, _state.prevAppState, "");
  } else if (eventType == "call_info") {
    String callNumber = String(_state.callState.callNumber);
    return buildPhoneStateEvent(eventType, AppState::Idle, AppState::Idle, callNumber);
  } else if (eventType == "dialing") {
    String currentNumber = String(_state.currentDialingNumber);
    return buildPhoneStateEvent(eventType, AppState::Idle, AppState::Idle, currentNumber);
  } else {
    // For other event types (ring, dnd, etc.), delegate to main method
    return buildPhoneStateEvent(eventType, AppState::Idle, AppState::Idle, "");
  }
}

JsonDocument IntegrationService::buildSystemEvent(const String &eventType) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", eventType);

  if (eventType == "stats") {
    addStatsInfo(obj);
  } else if (eventType == "status") {
    addSystemInfo(obj);
  }

  return doc;
}

JsonDocument IntegrationService::buildFullStateEvent(const String &callNumber,
                                                     bool isIncoming,
                                                     unsigned long startTime) {
  JsonDocument doc;
  // Snapshot diagnostic event (formerly full_state)
  JsonObject obj = createEventObject(doc, "diagnostic", "snapshot");
  addBasicDeviceInfo(obj);
  addPhoneStateInfo(obj);
  // Prefer tracked start ts if available
  unsigned long startTs = _currentCallStartTs > 0 ? _currentCallStartTs : startTime;
  addCallInfo(obj, callNumber, isIncoming, startTs);
  addSystemInfo(obj);
  addStatsInfo(obj);
  return doc;
}

JsonDocument IntegrationService::buildCurrentCallEvent(const String &eventType,
                                                       unsigned long duration) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", eventType);

  String callNumber = String(_state.callState.callNumber);

  if (eventType == "start") {
    unsigned long startTs = _currentCallStartTs > 0 ? _currentCallStartTs : millis();
    addCallInfo(obj, callNumber, false, startTs);
  } else if (eventType == "end") {
    if (duration > 0) {
      obj["durationMs"] = (uint32_t)(duration * 1000UL);
      obj["callStartTs"] = _currentCallStartTs;
    }
    addPhoneStateInfo(obj);
  } else if (eventType == "blocked") {
    obj["number"] = callNumber;
    obj["isIncoming"] = true;
    obj["callStartTs"] = _currentCallStartTs; // may be 0 if blocked pre-start
  }

  return doc;
}

JsonDocument IntegrationService::buildCurrentFullStateEvent() {
  // Extract current call info from state and delegate to the main method
  String callNumber = String(_state.callState.callNumber);
  if (!callNumber.isEmpty()) {
    return buildFullStateEvent(
        callNumber, false, millis()); // isIncoming info not available in state
  } else {
    return buildFullStateEvent("", false, 0);
  }
}

JsonDocument IntegrationService::buildSystemConfigEvent(const String &key,
                                                        const JsonVariant &newValue,
                                                        const JsonVariantConst &oldValue) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "config", "config_delta");
  obj["key"] = key;
  if (!oldValue.isNull()) {
    obj["oldValue"] = oldValue;
  }
  obj["newValue"] = newValue;
  return doc;
}

JsonDocument IntegrationService::buildAggregatedConfigDeltaEvent(
    const std::vector<std::tuple<String, JsonVariantConst, JsonVariantConst>> &changes) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "config", "config_delta");
  JsonArray arr = obj["changes"].to<JsonArray>();
  for (const auto &t : changes) {
    const String &key = std::get<0>(t);
    JsonVariantConst oldV = std::get<1>(t);
    JsonVariantConst newV = std::get<2>(t);
    JsonObject c = arr.add<JsonObject>();
    c["key"] = key;
    if (!oldV.isNull()) {
      c["oldValue"] = oldV;
    }
    c["newValue"] = newV;
  }
  return doc;
}

#endif // HOME_ASSISTANT_INTEGRATION || ANDROID_INTEGRATION