#ifdef HOME_ASSISTANT_INTEGRATION

#include "IntegrationService.h"
#include "../common/logger.h"
#include "../common/phoneNormalization.h"
#include "IntegrationLog.h"
#include "IntegrationLookup.h"
#include <algorithm>

namespace {
  constexpr const char *kDirectionIncoming = "incoming";
  constexpr const char *kDirectionOutgoing = "outgoing";
}

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
    std::function<IntegrationCallbackResult(const String &, bool)> callback) {
  _ringCallback = callback;
}

void IntegrationService::setDialDigitCallback(
    std::function<IntegrationCallbackResult(uint8_t, bool)> callback) {
  _dialDigitCallback = callback;
}

void IntegrationService::setSendDialedNumberCallback(
    std::function<IntegrationCallbackResult()> callback) {
  _sendDialedNumberCallback = callback;
}

void IntegrationService::setCallWaitingCallback(
    std::function<IntegrationCallbackResult()> callback) {
  _callWaitingCallback = callback;
}

void IntegrationService::setVolumeModeCallback(
    std::function<IntegrationCallbackResult(VolumeMode)> callback) {
  _volumeModeCallback = callback;
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

IntegrationCallbackResult IntegrationService::handleDialDigit(uint8_t digit, bool deferValidation) {
  if (digit > 9) {
    return IntegrationCallbackResult(false, "Digit must be between 0 and 9", "WEB_INVALID_DIGIT");
  }

  if (!_dialDigitCallback) {
    return IntegrationCallbackResult(
        false, "Dial digit callback not available", "WEB_SERVICE_UNAVAILABLE");
  }

  IntegrationCallbackResult result = _dialDigitCallback(digit, deferValidation);
  if (result.success) {
    INT_LOG_INFO("CORE", "Dial digit success %u (defer: %s)", 
                 static_cast<unsigned>(digit),
                 deferValidation ? "yes" : "no");
  } else {
    INT_LOG_ERROR("CORE",
                  "Dial digit %u failed: %s",
                  static_cast<unsigned>(digit),
                  result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleSendDialedNumber() {
  if (!_sendDialedNumberCallback) {
    return IntegrationCallbackResult(
        false, "Send dialed number callback not available", "WEB_SERVICE_UNAVAILABLE");
  }

  IntegrationCallbackResult result = _sendDialedNumberCallback();
  if (result.success) {
    INT_LOG_INFO("CORE", "Send dialed number success");
  } else {
    INT_LOG_ERROR("CORE", "Send dialed number failed: %s", result.errorMessage.c_str());
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

IntegrationCallbackResult IntegrationService::handleSetVolumeMode(VolumeMode mode) {
  INT_LOG_INFO(
      "CORE", "Volume mode request %s", mode == VolumeMode::Speaker ? "speaker" : "earpiece");

  if (!_volumeModeCallback) {
    return IntegrationCallbackResult(
        false, "Volume mode callback not available", "WEB_SERVICE_UNAVAILABLE");
  }

  IntegrationCallbackResult result = _volumeModeCallback(mode);
  if (result.success) {
    INT_LOG_INFO("CORE", "Volume mode set success");
  } else {
    INT_LOG_ERROR("CORE", "Volume mode set failed %s", result.errorMessage.c_str());
  }

  return result;
}

IntegrationCallbackResult IntegrationService::handleToggleVolumeMode() {
  INT_LOG_INFO("CORE", "Volume mode toggle request");

  VolumeMode current = _state.volumeMode;
  if (current != VolumeMode::Speaker && current != VolumeMode::Earpiece) {
    current = VolumeMode::Earpiece;
  }

  const VolumeMode target =
      (current == VolumeMode::Speaker) ? VolumeMode::Earpiece : VolumeMode::Speaker;
  return handleSetVolumeMode(target);
}

IntegrationCallbackResult IntegrationService::handleRingOperation(const String &pattern,
                                                                  bool force) {
  INT_LOG_INFO(
      "CORE", "Ring operation pattern %s (force=%s)", pattern.c_str(), force ? "true" : "false");

  if (!_ringCallback) {
    return IntegrationCallbackResult(false, "Ring callback not available");
  }

  IntegrationCallbackResult result = _ringCallback(pattern, force);
  if (result.success) {
    INT_LOG_INFO(
        "CORE", "Ring success pattern %s (force=%s)", pattern.c_str(), force ? "true" : "false");
  } else {
    INT_LOG_ERROR(
        "CORE", "Ring failed %s (force=%s)", result.errorMessage.c_str(), force ? "true" : "false");
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
    _config.setRingPattern("");
    INT_LOG_INFO("CORE", "Ring pattern cleared to device default");
  } else {
    _config.setRingPattern(pattern);
    INT_LOG_INFO("CORE", "Ring pattern %s", pattern.c_str());
  }

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

IntegrationCallbackResult IntegrationService::handleSetDialingConfig(const JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    return IntegrationCallbackResult(false, "Invalid JSON payload");
  }

  JsonObject obj = json.as<JsonObject>();
  JsonVariant defaultCodeVariant = obj["defaultCode"];
  if (defaultCodeVariant.isNull()) {
    return IntegrationCallbackResult(false, "Missing required field: defaultCode");
  }

  String requestedCode;
  if (defaultCodeVariant.is<const char *>()) {
    requestedCode = defaultCodeVariant.as<const char *>();
  } else if (defaultCodeVariant.is<String>()) {
    requestedCode = defaultCodeVariant.as<String>();
  } else {
    return IntegrationCallbackResult(false, "defaultCode must be a string");
  }

  requestedCode.trim();
  if (requestedCode.isEmpty()) {
    return IntegrationCallbackResult(false, "defaultCode cannot be empty");
  }

  const String previousCode = _config.getDefaultDialingCode();
  _config.setDefaultDialingCode(requestedCode);
  const String effectiveCode = _config.getDefaultDialingCode();

  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  JsonObject dialing = data["dialing"].to<JsonObject>();
  dialing["defaultCode"] = effectiveCode;
  if (!effectiveCode.isEmpty()) {
    dialing["defaultPrefix"] = String("+") + effectiveCode;
  } else {
    dialing["defaultPrefix"] = "";
  }
  dialing["previousCode"] = previousCode;

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
    String trimmedNumberStr = number;
    trimmedNumberStr.trim();
    const String normalizedCandidate = _config.normalizeNumber(trimmedNumberStr);
    const auto &entries = _config.getQuickDialEntries();
    auto it = std::find_if(
        entries.begin(), entries.end(), [&](const QuickDialEntry &qd) { return qd.code == code; });
    if (it != entries.end()) {
      entry["number"] = it->effectiveNumber();
      if (it->hasNormalized()) {
        entry["normalizedNumber"] = it->normalizedNumber;
      }
    } else if (!normalizedCandidate.isEmpty()) {
      entry["number"] = normalizedCandidate;
      entry["normalizedNumber"] = normalizedCandidate;
    }

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
                                                                     const String &name) {
  if (number.isEmpty()) {
    return IntegrationCallbackResult(false, "Number cannot be empty");
  }

  // Explicit conflict: cannot block a number that is currently a priority caller
  if (_config.isPriorityCaller(number)) {
    INT_LOG_WARN("CORE", "Blocked add rejected: %s is a priority caller", number.c_str());
    return IntegrationCallbackResult(false, "Number is a priority caller");
  }

  if (_config.addBlockedNumber(number, name)) {
    // Publish config change event
    // Config change event emitted externally (C2)

    // Create response data
    JsonDocument resultData;
    JsonObject data = resultData.to<JsonObject>();
    JsonObject entry = data["entry"].to<JsonObject>();
    entry["number"] = number;
    entry["name"] = name;
    String trimmedNumberStr = number;
    trimmedNumberStr.trim();
    const String normalizedCandidate = _config.normalizeNumber(trimmedNumberStr);
    if (!normalizedCandidate.isEmpty()) {
      const auto &blockedEntries = _config.getBlockedNumbers();
      auto it = std::find_if(blockedEntries.begin(),
                             blockedEntries.end(),
                             [&](const BlockedNumberEntry &blockedEntry) {
                               return blockedEntry.matchesNormalized(normalizedCandidate);
                             });
      if (it != blockedEntries.end() && it->hasNormalized()) {
        entry["number"] = it->effectiveNumber();
        entry["normalizedNumber"] = it->normalizedNumber;
      } else {
        entry["number"] = normalizedCandidate;
        entry["normalizedNumber"] = normalizedCandidate;
      }
    }

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

  String trimmedNumberStr = number;
  trimmedNumberStr.trim();
  const String normalizedCandidate = _config.normalizeNumber(trimmedNumberStr);
  const String &logNumber = normalizedCandidate.isEmpty() ? number : normalizedCandidate;

  INT_LOG_INFO("CORE",
               "Priority caller added %s total=%u",
               logNumber.c_str(),
               (unsigned)_config.getPriorityCallers().size());

  // Create response data
  JsonDocument resultData;
  JsonObject data = resultData.to<JsonObject>();
  JsonObject entry = data["entry"].to<JsonObject>();
  entry["number"] = number;
  entry["priority"] = true;
  if (!normalizedCandidate.isEmpty()) {
    const auto &priorityEntries = _config.getPriorityCallers();
    auto it = std::find_if(priorityEntries.begin(),
                           priorityEntries.end(),
                           [&](const PriorityCallerEntry &priorityEntry) {
                             return priorityEntry.matchesNormalized(normalizedCandidate);
                           });
    if (it != priorityEntries.end() && it->hasNormalized()) {
      entry["number"] = it->effectiveNumber();
      entry["normalizedNumber"] = it->normalizedNumber;
    } else {
      entry["number"] = normalizedCandidate;
      entry["normalizedNumber"] = normalizedCandidate;
    }
  }
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
    INT_LOG_WARN("CORE",
                 "Executing scheduled device %s",
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
  dnd["scheduled"] = dndConfig.scheduled;
  dnd["startMinute"] = dndConfig.startMinute;
  dnd["endMinute"] = dndConfig.endMinute;
  dnd["startHour"] = dndConfig.startHour;
  dnd["endHour"] = dndConfig.endHour;

  // Ring config (DS7 normalized)
  JsonObject ring = config["ring"].to<JsonObject>();
  ring["pattern"] = _config.getRingPattern();

  // Dialing config
  JsonObject dialing = config["dialing"].to<JsonObject>();
  const String defaultCode = _config.getDefaultDialingCode();
  dialing["defaultCode"] = defaultCode;
  if (!defaultCode.isEmpty()) {
    dialing["defaultPrefix"] = String("+") + defaultCode;
  } else {
    dialing["defaultPrefix"] = "";
  }
}

void IntegrationService::addStats(JsonObject &doc) {
  const CallStats &callStats = _stats.getCallStats();
  JsonObject stats = doc["stats"].to<JsonObject>();

  JsonObject calls = stats["calls"].to<JsonObject>();
  JsonObject totals = calls["totals"].to<JsonObject>();
  totals["total"] = callStats.totalCalls;
  totals["incoming"] = callStats.incomingCalls;
  totals["outgoing"] = callStats.outgoingCalls;
  totals["blocked"] = callStats.blockedCalls;
  totals["talkTimeSeconds"] = callStats.totalTalkTimeSeconds;

  CallRecord currentSnapshot = buildCurrentCallSnapshot(callStats.currentCall,
                                                        String(_state.callState.active.number),
                                                        _currentCallIsIncoming,
                                                        _state.callState.active.isPriority);
  unsigned long callStartTs = _currentCallStartTs;
  if (callStartTs == 0 && _state.callState.active.startedAtMs != 0UL) {
    callStartTs = _state.callState.active.startedAtMs;
  }
  uint32_t liveDurationSeconds = 0;
  if (!currentSnapshot.number.isEmpty() && callStartTs > 0 &&
      _state.newAppState == AppState::InCall) {
    liveDurationSeconds = (millis() - callStartTs) / 1000UL;
  }
  JsonObject currentCall = calls["currentCall"].to<JsonObject>();
  serializeCallRecord(
      currentCall, currentSnapshot, "active", callStartTs, liveDurationSeconds, true);

  LastCallRecord lastSnapshot = buildLastCallSnapshot(callStats.lastCall);
  JsonObject lastCall = calls["lastCall"].to<JsonObject>();
  serializeLastCallRecord(lastCall, lastSnapshot, true);

  JsonObject systemStats = stats["system"].to<JsonObject>();
  systemStats["resets"] = _stats.getResetCount();
}

void IntegrationService::addPhone(JsonObject &doc) {
  JsonObject phone = doc["phone"].to<JsonObject>();

  // Include current phone state snapshot alongside configuration lists (HA parity)
  addPhoneStateInfo(phone);

  const String defaultCode = _config.getDefaultDialingCode();
  JsonObject dialing = phone["dialing"].to<JsonObject>();
  dialing["defaultCode"] = defaultCode;
  if (!defaultCode.isEmpty()) {
    dialing["defaultPrefix"] = String("+") + defaultCode;
  } else {
    dialing["defaultPrefix"] = "";
  }

  // Quick dial entries
  JsonArray quickDial = phone["quickDial"].to<JsonArray>();
  for (const auto &entry : _config.getQuickDialEntries()) {
    JsonObject entryObj = quickDial.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
    if (entry.hasNormalized()) {
      entryObj["normalizedNumber"] = entry.normalizedNumber;
    }
  }

  // Blocked numbers
  JsonArray blocked = phone["blocked"].to<JsonArray>();
  for (const auto &entry : _config.getBlockedNumbers()) {
    JsonObject entryObj = blocked.add<JsonObject>();
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
    if (entry.hasNormalized()) {
      entryObj["normalizedNumber"] = entry.normalizedNumber;
    }
  }

  // Priority callers
  JsonArray priority = phone["priorityCallers"].to<JsonArray>();
  JsonArray priorityDetails = phone["priorityCallerDetails"].to<JsonArray>();
  for (const auto &entry : _config.getPriorityCallers()) {
    priority.add(entry.number);
    JsonObject obj = priorityDetails.add<JsonObject>();
    obj["number"] = entry.number;
    if (entry.hasNormalized()) {
      obj["normalizedNumber"] = entry.normalizedNumber;
    }
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
  obj["volumeMode"] = String(volumeModeToString(_state.volumeMode));
  obj["volumeModeCode"] = static_cast<int>(_state.volumeMode);
  obj["isSpeakerMode"] = (_state.volumeMode == VolumeMode::Speaker);

  // Add dialing buffer number if present (distinct from active call)
  if (_state.currentDialingNumber[0] != '\0') {
    obj["currentDialingNumber"] = _state.currentDialingNumber;
    String normalizedDial = PhoneNormalization::normalizePhoneNumber(
        String(_state.currentDialingNumber), _config.getDefaultDialingCode());
    if (!normalizedDial.isEmpty()) {
      obj["currentDialingNumberNormalized"] = normalizedDial;
    }
  }

  // Snapshot structured call data (current + last call)
  String activeNumber = String(_state.callState.active.number);
  addCallInfo(obj, activeNumber, _currentCallIsIncoming, _currentCallStartTs);
}

void IntegrationService::addBasicPhoneStatus(JsonObject &obj) {
  // Reuse addPhoneStateInfo to eliminate duplication
  addPhoneStateInfo(obj);
}

void IntegrationService::addCallInfo(JsonObject &obj,
                                     const String &callNumber,
                                     bool isIncoming,
                                     unsigned long startTime) {
  const CallLeg &activeLeg = _state.callState.active;
  const CallLeg &waitingLeg = _state.callState.waiting;

  // Build currentCall snapshot from StatsManager cache (now properly updated on leg swaps)
  CallRecord currentSnapshot = buildCurrentCallSnapshot(_stats.getCurrentCall(),
                                                        String(activeLeg.number),
                                                        _currentCallIsIncoming,
                                                        activeLeg.isPriority);

  unsigned long callStartTs = startTime > 0 ? startTime : _currentCallStartTs;
  if (callStartTs == 0 && activeLeg.startedAtMs != 0UL) {
    callStartTs = activeLeg.startedAtMs;
  }

  const bool currentCallActive = (!currentSnapshot.number.isEmpty() && callStartTs > 0 &&
                                  _state.newAppState == AppState::InCall);
  const unsigned long nowMs = millis();
  uint32_t liveDurationSeconds = 0;
  if (currentCallActive) {
    liveDurationSeconds = (nowMs - callStartTs) / 1000UL;
  }

  JsonObject currentCallObj = obj["currentCall"].to<JsonObject>();
  currentCallObj.clear();
  serializeCallRecord(
      currentCallObj, currentSnapshot, "active", callStartTs, liveDurationSeconds, true);

  currentCallObj["leg"] = "active";
  currentCallObj["callId"] = activeLeg.id;
  currentCallObj["isOnHold"] = activeLeg.isOnHold;
  currentCallObj["isBlocked"] = activeLeg.isBlocked;
  currentCallObj["durationMs"] = liveDurationSeconds * 1000UL;
  if (callStartTs > 0) {
    currentCallObj["durationMs"] = static_cast<uint32_t>(nowMs - callStartTs);
  }

  if (!currentSnapshot.number.isEmpty()) {
    obj["currentCallNumber"] = currentSnapshot.number;
    obj["currentCallName"] = currentSnapshot.name;
    obj["currentCallIsPriority"] = currentSnapshot.isPriority;
    obj["isIncomingCall"] = currentSnapshot.isIncoming;
    obj["currentCallId"] = activeLeg.id;
    obj["currentCallIsOnHold"] = activeLeg.isOnHold;
    obj["currentCallIsBlocked"] = activeLeg.isBlocked;

    if (callStartTs > 0) {
      obj["callStartTs"] = callStartTs;
      uint32_t durationMs = static_cast<uint32_t>(nowMs - callStartTs);
      obj["currentCallDurationMs"] = durationMs;
      obj["currentCallDurationSeconds"] = durationMs / 1000UL;
    } else {
      obj["callStartTs"] = 0;
      obj["currentCallDurationMs"] = 0;
      obj["currentCallDurationSeconds"] = 0;
    }

    String normalized = PhoneNormalization::normalizePhoneNumber(currentSnapshot.number,
                                                                 _config.getDefaultDialingCode());
    if (!normalized.isEmpty()) {
      obj["currentCallNumberNormalized"] = normalized;
    } else {
      obj.remove("currentCallNumberNormalized");
    }
  } else {
    obj["currentCallNumber"] = "";
    obj["currentCallName"] = "";
    obj["currentCallIsPriority"] = false;
    obj["isIncomingCall"] = false;
    obj["currentCallId"] = -1;
    obj["currentCallIsOnHold"] = false;
    obj["currentCallIsBlocked"] = false;
    obj["callStartTs"] = 0;
    obj["currentCallDurationMs"] = 0;
    obj["currentCallDurationSeconds"] = 0;
    obj.remove("currentCallNumberNormalized");
  }

  JsonObject waitingCallObj = obj["waitingCall"].to<JsonObject>();
  waitingCallObj.clear();

  const bool hasWaiting = waitingLeg.isValid();
  uint32_t waitingDurationSeconds = 0;
  if (waitingLeg.startedAtMs != 0UL) {
    waitingDurationSeconds = (nowMs - waitingLeg.startedAtMs) / 1000UL;
  }

  if (hasWaiting) {
    CallRecord waitingSnapshot;
    waitingSnapshot.number = String(waitingLeg.number);
    waitingSnapshot.isIncoming = waitingLeg.isIncoming;
    waitingSnapshot.isPriority = waitingLeg.isPriority;
    waitingSnapshot.durationSeconds = waitingDurationSeconds;
    if (!waitingSnapshot.number.isEmpty()) {
      waitingSnapshot.name = resolveCallerName(waitingSnapshot.number);
    }

    serializeCallRecord(waitingCallObj,
                        waitingSnapshot,
                        "available",
                        waitingLeg.startedAtMs,
                        waitingDurationSeconds,
                        true);

    waitingCallObj["leg"] = "waiting";
    waitingCallObj["callId"] = waitingLeg.id;
    waitingCallObj["isOnHold"] = waitingLeg.isOnHold;
    waitingCallObj["isBlocked"] = waitingLeg.isBlocked;
    waitingCallObj["durationMs"] = waitingLeg.startedAtMs > 0
                                       ? static_cast<uint32_t>(nowMs - waitingLeg.startedAtMs)
                                       : waitingDurationSeconds * 1000UL;

    obj["waitingCallNumber"] = waitingSnapshot.number;
    obj["waitingCallName"] = waitingSnapshot.name;
    obj["waitingCallIsPriority"] = waitingLeg.isPriority;
    obj["waitingCallIsIncoming"] = waitingLeg.isIncoming;
    obj["waitingCallIsBlocked"] = waitingLeg.isBlocked;
    obj["waitingCallIsOnHold"] = waitingLeg.isOnHold;
    obj["waitingCallId"] = waitingLeg.id;

    if (waitingLeg.startedAtMs > 0) {
      obj["waitingCallStartTs"] = waitingLeg.startedAtMs;
      obj["waitingCallDurationMs"] = static_cast<uint32_t>(nowMs - waitingLeg.startedAtMs);
    } else {
      obj["waitingCallStartTs"] = 0;
      obj["waitingCallDurationMs"] = 0;
    }

    String waitingNormalized = PhoneNormalization::normalizePhoneNumber(
        waitingSnapshot.number, _config.getDefaultDialingCode());
    if (!waitingNormalized.isEmpty()) {
      obj["waitingCallNumberNormalized"] = waitingNormalized;
    } else {
      obj.remove("waitingCallNumberNormalized");
    }
  } else {
    waitingCallObj["available"] = false;
    waitingCallObj["leg"] = "waiting";
    waitingCallObj["callId"] = -1;
    waitingCallObj["isOnHold"] = false;
    waitingCallObj["isBlocked"] = false;
    waitingCallObj["isPriority"] = false;
    waitingCallObj["isIncoming"] = false;
    waitingCallObj["durationSeconds"] = 0;
    waitingCallObj["durationMs"] = 0;

    obj["waitingCallNumber"] = "";
    obj["waitingCallName"] = "";
    obj["waitingCallIsPriority"] = false;
    obj["waitingCallIsIncoming"] = false;
    obj["waitingCallIsBlocked"] = false;
    obj["waitingCallIsOnHold"] = false;
    obj["waitingCallId"] = -1;
    obj["waitingCallStartTs"] = 0;
    obj["waitingCallDurationMs"] = 0;
    obj.remove("waitingCallNumberNormalized");
  }

  LastCallRecord lastSnapshot = buildLastCallSnapshot(_stats.getLastCall());
  JsonObject lastCallObj = obj["lastCall"].to<JsonObject>();
  lastCallObj.clear();
  serializeLastCallRecord(lastCallObj, lastSnapshot, true);

  if (!lastSnapshot.number.isEmpty()) {
    obj["lastCallNumber"] = lastSnapshot.number;
    obj["lastCallName"] = lastSnapshot.name;
    obj["lastCallResult"] = lastSnapshot.result;
    obj["lastCallIsIncoming"] = lastSnapshot.isIncoming;
    obj["lastCallIsPriority"] = lastSnapshot.isPriority;
    obj["lastCallDurationSeconds"] = lastSnapshot.durationSeconds;

    String normalizedLast = PhoneNormalization::normalizePhoneNumber(
        lastSnapshot.number, _config.getDefaultDialingCode());
    if (!normalizedLast.isEmpty()) {
      obj["lastCallNumberNormalized"] = normalizedLast;
    }
  } else {
    obj["lastCallNumber"] = "";
    obj["lastCallName"] = "";
    obj["lastCallResult"] = "";
    obj["lastCallIsIncoming"] = false;
    obj["lastCallIsPriority"] = false;
    obj["lastCallDurationSeconds"] = 0;
    obj.remove("lastCallNumberNormalized");
  }

  obj["callWaitingId"] = waitingLeg.id;
  obj["callWaitingAvailable"] = hasWaiting;
  obj["callWaitingOnHold"] = waitingLeg.isOnHold;
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

  CallRecord currentSnapshot = buildCurrentCallSnapshot(callStats.currentCall,
                                                        String(_state.callState.active.number),
                                                        _currentCallIsIncoming,
                                                        _state.callState.active.isPriority);
  unsigned long callStartTs = _currentCallStartTs;
  if (callStartTs == 0 && _state.callState.active.startedAtMs != 0UL) {
    callStartTs = _state.callState.active.startedAtMs;
  }
  uint32_t liveDurationSeconds = 0;
  if (!currentSnapshot.number.isEmpty() && callStartTs > 0 &&
      _state.newAppState == AppState::InCall) {
    liveDurationSeconds = (millis() - callStartTs) / 1000UL;
  }

  JsonObject currentCall = calls["currentCall"].to<JsonObject>();
  serializeCallRecord(
      currentCall, currentSnapshot, "active", callStartTs, liveDurationSeconds, true);

  LastCallRecord lastSnapshot = buildLastCallSnapshot(callStats.lastCall);
  JsonObject lastCall = calls["lastCall"].to<JsonObject>();
  serializeLastCallRecord(lastCall, lastSnapshot, true);

  JsonObject systemStats = obj["system"].to<JsonObject>();
  systemStats["resets"] = _stats.getResetCount();
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
  String numberHint = number;
  if (numberHint.isEmpty() && _state.callState.active.number[0] != '\0') {
    numberHint = String(_state.callState.active.number);
  }

  if (eventType == "start") {
    CallRecord currentSnapshot = buildCurrentCallSnapshot(
        _stats.getCurrentCall(), numberHint, isIncoming, _state.callState.active.isPriority);
    unsigned long callStartTs = _currentCallStartTs > 0 ? _currentCallStartTs : 0;
    if (callStartTs == 0 && _state.callState.active.startedAtMs != 0UL) {
      callStartTs = _state.callState.active.startedAtMs;
    }
    if (callStartTs == 0) {
      callStartTs = millis();
    }
    uint32_t liveDurationSeconds = 0;
    if (!currentSnapshot.number.isEmpty() && callStartTs > 0) {
      liveDurationSeconds = (millis() - callStartTs) / 1000UL;
    }

    JsonObject currentCall = obj["currentCall"].to<JsonObject>();
    serializeCallRecord(
        currentCall, currentSnapshot, "active", callStartTs, liveDurationSeconds, true);

    if (!currentSnapshot.number.isEmpty()) {
      obj["number"] = currentSnapshot.number;
      String normalized = PhoneNormalization::normalizePhoneNumber(currentSnapshot.number,
                                                                   _config.getDefaultDialingCode());
      if (!normalized.isEmpty()) {
        obj["normalizedNumber"] = normalized;
      }
    }
    if (!currentSnapshot.name.isEmpty()) {
      obj["currentCallName"] = currentSnapshot.name;
    }

    obj["isIncoming"] = currentSnapshot.isIncoming;
    obj["isPriority"] = currentSnapshot.isPriority;
    obj["callStartTs"] = callStartTs;
  } else {
    LastCallRecord lastSnapshot = buildLastCallSnapshot(_stats.getLastCall());
    JsonObject lastCall = obj["lastCall"].to<JsonObject>();
    serializeLastCallRecord(lastCall, lastSnapshot, true);

    if (!lastSnapshot.number.isEmpty()) {
      obj["number"] = lastSnapshot.number;
      String normalized = PhoneNormalization::normalizePhoneNumber(lastSnapshot.number,
                                                                   _config.getDefaultDialingCode());
      if (!normalized.isEmpty()) {
        obj["normalizedNumber"] = normalized;
      }
    }
    if (!lastSnapshot.name.isEmpty()) {
      obj["lastCallName"] = lastSnapshot.name;
    }
    if (!lastSnapshot.result.isEmpty()) {
      obj["result"] = lastSnapshot.result;
    }

    obj["isIncoming"] = lastSnapshot.isIncoming;
    obj["isPriority"] = lastSnapshot.isPriority;
    obj["durationSeconds"] = lastSnapshot.durationSeconds;

    if (eventType == "end") {
      if (_currentCallStartTs > 0) {
        obj["callStartTs"] = _currentCallStartTs;
      }
      if (duration > 0) {
        obj["durationMs"] = static_cast<uint32_t>(duration * 1000UL);
      }
    } else if (eventType == "blocked") {
      obj["isIncoming"] = true;
      if (lastSnapshot.result.isEmpty()) {
        obj["result"] = "blocked";
      }
    }
  }
  addPhoneStateInfo(obj);
  return doc;
}

String IntegrationService::resolveCallerName(const String &number) const {
  return IntegrationLookup::lookupCallerName(_config, number);
}

CallRecord IntegrationService::buildCurrentCallSnapshot(const CallRecord &base,
                                                        const String &numberHint,
                                                        bool incomingHint,
                                                        bool priorityHint) const {
  CallRecord snapshot = base;

  if (snapshot.number.isEmpty()) {
    if (!numberHint.isEmpty()) {
      snapshot.number = numberHint;
    } else if (_state.callState.active.number[0] != '\0') {
      snapshot.number = String(_state.callState.active.number);
    }
  }

  if (base.number.isEmpty()) {
    snapshot.isIncoming = incomingHint;
    snapshot.isPriority = priorityHint;
  }

  if (snapshot.name.isEmpty() && !snapshot.number.isEmpty()) {
    snapshot.name = resolveCallerName(snapshot.number);
  }

  // Clear durationSeconds so serializeCallRecord always uses the live calculated value
  // This prevents stale duration from persisting after call waiting leg swaps
  snapshot.durationSeconds = 0;

  return snapshot;
}

LastCallRecord IntegrationService::buildLastCallSnapshot(const LastCallRecord &base) const {
  LastCallRecord snapshot = base;

  if (snapshot.name.isEmpty() && !snapshot.number.isEmpty()) {
    snapshot.name = resolveCallerName(snapshot.number);
  }

  return snapshot;
}

void IntegrationService::serializeCallRecord(JsonObject &target,
                                             const CallRecord &record,
                                             const char *presenceKey,
                                             unsigned long startTs,
                                             uint32_t durationOverride,
                                             bool includeNormalized) const {
  const bool hasNumber = !record.number.isEmpty();
  if (presenceKey != nullptr) {
    target[presenceKey] = hasNumber;
  }

  if (!record.name.isEmpty()) {
    target["name"] = record.name;
  }

  if (hasNumber) {
    target["number"] = record.number;

    if (includeNormalized) {
      String normalized =
          PhoneNormalization::normalizePhoneNumber(record.number, _config.getDefaultDialingCode());
      if (!normalized.isEmpty()) {
        target["normalizedNumber"] = normalized;
      }
    }

    target["direction"] = record.isIncoming ? kDirectionIncoming : kDirectionOutgoing;
    target["isIncoming"] = record.isIncoming;
    target["isPriority"] = record.isPriority;

    uint32_t durationValue = durationOverride > 0 ? durationOverride : record.durationSeconds;
    target["durationSeconds"] = durationValue;
    target["durationMs"] = durationValue * 1000UL;

    if (startTs > 0) {
      target["startTs"] = startTs;
    }
  } else {
    // When no active call is present, normalize boolean fields for template consumers.
    target["isPriority"] = false;
    target["isIncoming"] = false;
    target["durationSeconds"] = 0;
    target["durationMs"] = 0;
  }
}

void IntegrationService::serializeLastCallRecord(JsonObject &target,
                                                 const LastCallRecord &record,
                                                 bool includeNormalized) const {
  serializeCallRecord(target, record, "available", 0, 0, includeNormalized);
  if (!record.result.isEmpty()) {
    target["result"] = record.result;
  }
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
    addPhoneStateInfo(obj);
    // Snapshot only (no fabricated start time here)
    addCallInfo(obj, currentNumber, _currentCallIsIncoming, 0);
  } else if (eventType == "dialing") {
    addPhoneStateInfo(obj);
    obj["currentDialingNumber"] = currentNumber;
  } else if (eventType == "ring") {
    addPhoneStateInfo(obj);
    obj["isRinging"] = true; // Assuming true when creating ring event
  } else if (eventType == "dnd") {
    addPhoneStateInfo(obj);
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
    String callNumber = String(_state.callState.active.number);
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

  String callNumber = String(_state.callState.active.number);

  if (eventType == "start") {
    unsigned long startTs = _currentCallStartTs > 0 ? _currentCallStartTs : 0;
    if (startTs == 0 && _state.callState.active.startedAtMs != 0UL) {
      startTs = _state.callState.active.startedAtMs;
    }
    if (startTs == 0) {
      startTs = millis();
    }
    addCallInfo(obj, callNumber, _currentCallIsIncoming, startTs);
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
  String callNumber = String(_state.callState.active.number);
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

#endif // HOME_ASSISTANT_INTEGRATION