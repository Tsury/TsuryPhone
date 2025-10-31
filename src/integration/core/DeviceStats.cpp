#ifdef HOME_ASSISTANT_INTEGRATION

#include "DeviceStats.h"
#include "../../common/logger.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_timer.h>

namespace {
  constexpr const char *kDirectionIncoming = "incoming";
  constexpr const char *kDirectionOutgoing = "outgoing";
  constexpr const char *kResultAnswered = "answered";
  constexpr const char *kResultMissed = "missed";
  constexpr const char *kResultBlocked = "blocked";
  constexpr const char *kResultUnanswered = "unanswered";

  String normalizeDirection(const String &value, bool defaultIncoming) {
    if (value.equalsIgnoreCase(kDirectionIncoming)) {
      return kDirectionIncoming;
    }
    if (value.equalsIgnoreCase(kDirectionOutgoing)) {
      return kDirectionOutgoing;
    }
    return defaultIncoming ? String(kDirectionIncoming) : String(kDirectionOutgoing);
  }
}

const char *DeviceStats::kStatsFilePath = "/stats.json";

DeviceStats::DeviceStats() : _systemStartTime(0), _callStartTime(0) {
  recordSystemStart();
}

bool DeviceStats::init() {
  if (!load()) {
    Logger::infoln(F("No existing stats found, starting fresh"));
  }

  // Increment reset count on every initialization
  incrementResetCount();

  return true;
}

bool DeviceStats::save() {
  JsonDocument doc;

  // Stats hierarchy: stats.calls.totals.*
  JsonObject stats = doc["stats"].to<JsonObject>();
  JsonObject calls = stats["calls"].to<JsonObject>();
  JsonObject totals = calls["totals"].to<JsonObject>();

  totals["calls"] = _callStats.totalCalls;
  totals["incoming"] = _callStats.incomingCalls;
  totals["outgoing"] = _callStats.outgoingCalls;
  totals["blocked"] = _callStats.blockedCalls;
  totals["talkTime"] = _callStats.totalTalkTimeSeconds;

  // Save current call snapshot when active
  if (!_callStats.currentCall.number.isEmpty()) {
    JsonObject currentCall = calls["currentCall"].to<JsonObject>();
    currentCall["number"] = _callStats.currentCall.number;
    if (!_callStats.currentCall.name.isEmpty()) {
      currentCall["name"] = _callStats.currentCall.name;
    }
    currentCall["direction"] =
        _callStats.currentCall.isIncoming ? kDirectionIncoming : kDirectionOutgoing;
    currentCall["isPriority"] = _callStats.currentCall.isPriority;
    currentCall["durationSeconds"] = _callStats.currentCall.durationSeconds;
  }

  // Save last call snapshot
  if (!_callStats.lastCall.number.isEmpty()) {
    JsonObject lastCall = calls["lastCall"].to<JsonObject>();
    lastCall["number"] = _callStats.lastCall.number;
    if (!_callStats.lastCall.name.isEmpty()) {
      lastCall["name"] = _callStats.lastCall.name;
    }
    lastCall["direction"] =
        _callStats.lastCall.isIncoming ? kDirectionIncoming : kDirectionOutgoing;
    lastCall["isPriority"] = _callStats.lastCall.isPriority;
    lastCall["durationSeconds"] = _callStats.lastCall.durationSeconds;
    if (!_callStats.lastCall.result.isEmpty()) {
      lastCall["result"] = _callStats.lastCall.result;
    }
  }

  // System stats: stats.system.*
  JsonObject system = stats["system"].to<JsonObject>();
  system["resets"] = _resetCount;

  File file = SPIFFS.open(kStatsFilePath, "w");
  if (!file) {
    Logger::errorln(F("Failed to open stats file for writing"));
    return false;
  }

  if (serializeJson(doc, file) == 0) {
    Logger::errorln(F("Failed to write stats to file"));
    file.close();
    return false;
  }

  file.close();
  _revision++;

  return true;
}

bool DeviceStats::load() {
  if (!SPIFFS.exists(kStatsFilePath)) {
    return false;
  }

  File file = SPIFFS.open(kStatsFilePath, "r");
  if (!file) {
    Logger::errorln(F("Failed to open stats file for reading"));
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Logger::errorln(F("Failed to parse stats file: %s"), error.c_str());
    return false;
  }

  // Load hierarchical format: stats.calls.totals.*
  if (doc["stats"]["calls"]["totals"]) {
    JsonObject totals = doc["stats"]["calls"]["totals"];
    _callStats.totalCalls = totals["calls"] | 0;
    _callStats.incomingCalls = totals["incoming"] | 0;
    _callStats.outgoingCalls = totals["outgoing"] | 0;
    _callStats.blockedCalls = totals["blocked"] | 0;
    _callStats.totalTalkTimeSeconds = totals["talkTime"] | 0;
  }

  // Load current call snapshot
  if (doc["stats"]["calls"]["currentCall"].is<JsonObject>()) {
    JsonObject currentCall = doc["stats"]["calls"]["currentCall"];
    _callStats.currentCall.number = currentCall["number"].as<String>();
    _callStats.currentCall.name = currentCall["name"].as<String>();
    const String direction = currentCall["direction"].as<String>();
    _callStats.currentCall.isIncoming = normalizeDirection(direction, true) == kDirectionIncoming;
    _callStats.currentCall.isPriority = currentCall["isPriority"].as<bool>();
    _callStats.currentCall.durationSeconds = currentCall["durationSeconds"].as<uint32_t>();
  } else {
    _callStats.currentCall.clear();
  }

  // Load last call snapshot
  if (doc["stats"]["calls"]["lastCall"].is<JsonObject>()) {
    JsonObject lastCall = doc["stats"]["calls"]["lastCall"];
    _callStats.lastCall.number = lastCall["number"].as<String>();
    _callStats.lastCall.name = lastCall["name"].as<String>();
    const String direction = lastCall["direction"].as<String>();
    _callStats.lastCall.isIncoming = normalizeDirection(direction, true) == kDirectionIncoming;
    _callStats.lastCall.isPriority = lastCall["isPriority"].as<bool>();
    _callStats.lastCall.durationSeconds = lastCall["durationSeconds"].as<uint32_t>();
    _callStats.lastCall.result = lastCall["result"].as<String>();
  } else {
    _callStats.lastCall.clear();
  }

  // Load system stats: stats.system.*
  if (doc["stats"]["system"]) {
    JsonObject systemStats = doc["stats"]["system"];
    _resetCount = systemStats["resets"] | 0;
  }

  Logger::infoln(F("Statistics loaded successfully"));
  return true;
}

void DeviceStats::reset() {
  Logger::warnln(F("DeviceStats: clearing persisted statistics"));

  _callStats = CallStats();
  _callStartTime = 0;
  _resetCount = 0;
  _revision = 0;
  _systemStartTime = esp_timer_get_time();

  if (SPIFFS.exists(kStatsFilePath) && !SPIFFS.remove(kStatsFilePath)) {
    Logger::errorln(F("Failed to remove stats file during factory reset"));
  }
}

void DeviceStats::beginCall(const String &number,
                            const String &name,
                            bool isIncoming,
                            bool isPriority) {
  _callStats.currentCall.number = number;
  _callStats.currentCall.name = name;
  _callStats.currentCall.isIncoming = isIncoming;
  _callStats.currentCall.isPriority = isPriority;
  _callStats.currentCall.durationSeconds = 0;

  if (isIncoming) {
    _callStats.incomingCalls++;
  } else {
    _callStats.outgoingCalls++;
  }
  _callStats.totalCalls++;

  _callStartTime = millis();
  save();
}

void DeviceStats::updateCurrentCall(const String &number,
                                    const String &name,
                                    bool isIncoming,
                                    bool isPriority) {
  // Update the current call record WITHOUT incrementing counters or resetting start time
  // Used for call waiting leg swaps where we're still in the same "call session"
  _callStats.currentCall.number = number;
  _callStats.currentCall.name = name;
  _callStats.currentCall.isIncoming = isIncoming;
  _callStats.currentCall.isPriority = isPriority;
  // Note: durationSeconds and _callStartTime are preserved
  save();
}

void DeviceStats::finalizeCurrentCall(const String &result) {
  if (_callStats.currentCall.number.isEmpty()) {
    return;
  }

  if (_callStartTime != 0) {
    uint32_t callDuration = (millis() - _callStartTime) / 1000;
    _callStats.currentCall.durationSeconds = callDuration;
    if (result.equalsIgnoreCase(kResultAnswered)) {
      _callStats.totalTalkTimeSeconds += callDuration;
    }
  }

  storeLastCallFromCurrent(result);
  _callStats.currentCall.clear();
  _callStartTime = 0;
  save();
}

void DeviceStats::recordBlockedCall(const String &number, const String &name, bool isPriority) {
  _callStats.blockedCalls++;
  _callStats.incomingCalls++;
  _callStats.totalCalls++;
  storeStandaloneLastCall(number, name, true, isPriority, 0, String(kResultBlocked));
  Logger::infoln(F("Blocked call from: %s"), number.c_str());
  save();
}

void DeviceStats::recordMissedIncomingCall(const String &number,
                                           const String &name,
                                           bool isPriority) {
  _callStats.incomingCalls++;
  _callStats.totalCalls++;
  storeStandaloneLastCall(number, name, true, isPriority, 0, String(kResultMissed));
  save();
}

void DeviceStats::recordUnansweredOutgoingCall(const String &number,
                                               const String &name,
                                               bool isPriority) {
  _callStats.outgoingCalls++;
  _callStats.totalCalls++;
  storeStandaloneLastCall(number, name, false, isPriority, 0, String(kResultUnanswered));
  save();
}

void DeviceStats::clearCurrentCall() {
  _callStats.currentCall.clear();
  _callStartTime = 0;
  save();
}

void DeviceStats::storeLastCallFromCurrent(const String &result) {
  _callStats.lastCall.name = _callStats.currentCall.name;
  _callStats.lastCall.number = _callStats.currentCall.number;
  _callStats.lastCall.isIncoming = _callStats.currentCall.isIncoming;
  _callStats.lastCall.isPriority = _callStats.currentCall.isPriority;
  _callStats.lastCall.durationSeconds = _callStats.currentCall.durationSeconds;
  _callStats.lastCall.result = result;
}

void DeviceStats::storeStandaloneLastCall(const String &number,
                                          const String &name,
                                          bool isIncoming,
                                          bool isPriority,
                                          uint32_t durationSeconds,
                                          const String &result) {
  _callStats.lastCall.number = number;
  _callStats.lastCall.name = name;
  _callStats.lastCall.isIncoming = isIncoming;
  _callStats.lastCall.isPriority = isPriority;
  _callStats.lastCall.durationSeconds = durationSeconds;
  _callStats.lastCall.result = result;
}

uint32_t DeviceStats::getUptime() const {
  return (esp_timer_get_time() - _systemStartTime) / 1000000;
}

uint32_t DeviceStats::getFreeHeap() const {
  return ESP.getFreeHeap();
}

int DeviceStats::getRSSI() const {
  return WiFi.RSSI();
}

void DeviceStats::recordSystemStart() {
  _systemStartTime = esp_timer_get_time();
}

void DeviceStats::incrementResetCount() {
  _resetCount++;
  save();
}

#endif // HOME_ASSISTANT_INTEGRATION