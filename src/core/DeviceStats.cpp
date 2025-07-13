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
    return save();
  }
  return true;
}

bool DeviceStats::save() {
  JsonDocument doc;

  doc["totalCalls"] = _callStats.totalCalls;
  doc["incomingCalls"] = _callStats.incomingCalls;
  doc["outgoingCalls"] = _callStats.outgoingCalls;
  doc["blockedCalls"] = _callStats.blockedCalls;
  doc["totalTalkTimeSeconds"] = _callStats.totalTalkTimeSeconds;
  doc["lastCall"] = _callStats.lastCall;

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

  _callStats.totalCalls = doc["totalCalls"] | 0;
  _callStats.incomingCalls = doc["incomingCalls"] | 0;
  _callStats.outgoingCalls = doc["outgoingCalls"] | 0;
  _callStats.blockedCalls = doc["blockedCalls"] | 0;
  _callStats.totalTalkTimeSeconds = doc["totalTalkTimeSeconds"] | 0;
  _callStats.lastCall = doc["lastCall"].as<String>();

  Logger::infoln(F("Statistics loaded successfully"));
  return true;
}

void DeviceStats::recordIncomingCall(const String &number) {
  _callStats.incomingCalls++;
  _callStats.totalCalls++;
  _callStats.lastCall = "Incoming - " + number;
  save();
}

void DeviceStats::recordOutgoingCall(const String &number) {
  _callStats.outgoingCalls++;
  _callStats.totalCalls++;
  _callStats.lastCall = "Outgoing - " + number;
  save();
}

void DeviceStats::recordBlockedCall(const String &number) {
  _callStats.blockedCalls++;
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
