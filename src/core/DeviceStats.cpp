#include "DeviceStats.h"
#include "../common/logger.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

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

  // Save lastCall as object: stats.calls.lastCall.*
  JsonObject lastCall = calls["lastCall"].to<JsonObject>();
  lastCall["number"] = _callStats.lastCall.number;
  lastCall["type"] = _callStats.lastCall.type;

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

  // Load lastCall: stats.calls.lastCall.*
  if (doc["stats"]["calls"]["lastCall"].is<JsonObject>()) {
    JsonObject lastCall = doc["stats"]["calls"]["lastCall"];
    _callStats.lastCall.number = lastCall["number"].as<String>();
    _callStats.lastCall.type = lastCall["type"].as<String>();
  }

  // Load system stats: stats.system.*
  if (doc["stats"]["system"]) {
    JsonObject systemStats = doc["stats"]["system"];
    _resetCount = systemStats["resets"] | 0;
  }

  Logger::infoln(F("Statistics loaded successfully"));
  return true;
}

void DeviceStats::recordIncomingCall(const String &number) {
  _callStats.incomingCalls++;
  _callStats.totalCalls++;
  _callStats.lastCall = LastCallInfo(number, "incoming");
  save();
}

void DeviceStats::recordOutgoingCall(const String &number) {
  _callStats.outgoingCalls++;
  _callStats.totalCalls++;
  _callStats.lastCall = LastCallInfo(number, "outgoing");
  save();
}

void DeviceStats::recordBlockedCall(const String &number) {
  _callStats.blockedCalls++;
  _callStats.lastCall = LastCallInfo(number, "blocked");
  save();
  Logger::infoln(F("Blocked call from: %s"), number.c_str());
}

void DeviceStats::recordCallStart() {
  _callStartTime = millis();
}

void DeviceStats::recordCallEnd() {
  if (_callStartTime != 0) {
    updateTalkTime();
    _callStartTime = 0;
    save();
  }
}

void DeviceStats::updateTalkTime() {
  if (_callStartTime != 0) {
    uint32_t callDuration = (millis() - _callStartTime) / 1000;
    _callStats.totalTalkTimeSeconds += callDuration;
  }
}

uint32_t DeviceStats::getUptime() const {
  return (millis() - _systemStartTime) / 1000;
}

uint32_t DeviceStats::getFreeHeap() const {
  return ESP.getFreeHeap();
}

int DeviceStats::getRSSI() const {
  return WiFi.RSSI();
}

void DeviceStats::recordSystemStart() {
  _systemStartTime = millis();
}

void DeviceStats::incrementResetCount() {
  _resetCount++;
  save();
}
