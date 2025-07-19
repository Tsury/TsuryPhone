#include "DeviceConfig.h"
#include "../common/logger.h"
#include "../config.h"
#include "../generated/phoneBook.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

const char *DeviceConfig::kConfigFilePath = "/config.json";
const char *DeviceConfig::kDefaultRingPattern = "500,500,500,500x3";

DeviceConfig::DeviceConfig() : _resetCount(0) {
  _deviceId = generateDeviceId();
  // Note: Don't call initializeDefaults() here - only call it when no config exists
}

bool DeviceConfig::init() {
  Logger::infoln(F("Initializing device configuration"));

  if (!SPIFFS.begin(true)) {
    Logger::errorln(F("Failed to initialize SPIFFS"));
    return false;
  }

  if (!load()) {
    Logger::infoln(F("No existing config found, initializing with defaults"));
    initializeDefaults();
  } else {
    Logger::infoln(F("Configuration loaded successfully"));
  }

  incrementResetCount();
  return true;
}

bool DeviceConfig::save() {
  JsonDocument doc;

  // Device info
  JsonObject device = doc["device"].to<JsonObject>();
  device["resetCount"] = _resetCount;

  // Audio config
  JsonObject audio = doc["audio"].to<JsonObject>();
  audio["earpieceVolume"] = _audioConfig.earpieceVolume;
  audio["earpieceGain"] = _audioConfig.earpieceGain;
  audio["speakerVolume"] = _audioConfig.speakerVolume;
  audio["speakerGain"] = _audioConfig.speakerGain;

  // DND config
  JsonObject dnd = doc["dnd"].to<JsonObject>();
  dnd["force"] = _dndConfig.force;
  dnd["scheduled"] = _dndConfig.scheduled;
  dnd["startHour"] = _dndConfig.startHour;
  dnd["startMinute"] = _dndConfig.startMinute;
  dnd["endHour"] = _dndConfig.endHour;
  dnd["endMinute"] = _dndConfig.endMinute;

  // Quick dial entries
  JsonObject quickDial = doc["quickDial"].to<JsonObject>();
  for (const auto &entry : _quickDialEntries) {
    quickDial[entry.first] = entry.second;
  }

  // Blocked numbers
  JsonArray blocked = doc["blockedNumbers"].to<JsonArray>();
  for (const String &number : _blockedNumbers) {
    blocked.add(number);
  }

  // Webhook actions
  JsonObject webhooks = doc["webhookActions"].to<JsonObject>();
  for (const auto &action : _webhookActions) {
    webhooks[action.first] = action.second;
  }

  // Ring pattern
  doc["ringPattern"] = _ringPattern;

  // Home Assistant URL
  doc["homeAssistantUrl"] = _homeAssistantUrl;

  File file = SPIFFS.open(kConfigFilePath, "w");
  if (!file) {
    Logger::errorln(F("Failed to open config file for writing"));
    return false;
  }

  if (serializeJson(doc, file) == 0) {
    Logger::errorln(F("Failed to write config to file"));
    file.close();
    return false;
  }

  file.close();
  Logger::infoln(F("Configuration saved successfully"));
  return true;
}

bool DeviceConfig::load() {
  if (!SPIFFS.exists(kConfigFilePath)) {
    Logger::infoln(F("Config file does not exist: %s"), kConfigFilePath);
    return false;
  }

  File file = SPIFFS.open(kConfigFilePath, "r");
  if (!file) {
    Logger::errorln(F("Failed to open config file for reading"));
    return false;
  }

  String fileContent = file.readString();
  file.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, fileContent);

  if (error) {
    Logger::errorln(F("Failed to parse config file: %s"), error.c_str());
    return false;
  }

  // Device info
  if (doc["device"]["resetCount"].is<int>()) {
    _resetCount = doc["device"]["resetCount"];
  }

  // Audio config
  if (doc["audio"]) {
    JsonObject audio = doc["audio"];
    if (audio["earpieceVolume"].is<int>()) {
      _audioConfig.earpieceVolume = audio["earpieceVolume"];
    }
    if (audio["earpieceGain"].is<int>()) {
      _audioConfig.earpieceGain = audio["earpieceGain"];
    }
    if (audio["speakerVolume"].is<int>()) {
      _audioConfig.speakerVolume = audio["speakerVolume"];
    }
    if (audio["speakerGain"].is<int>()) {
      _audioConfig.speakerGain = audio["speakerGain"];
    }
  }

  // DND config
  if (doc["dnd"]) {
    JsonObject dnd = doc["dnd"];
    if (dnd["force"].is<bool>()) {
      _dndConfig.force = dnd["force"];
    }
    if (dnd["scheduled"].is<bool>()) {
      _dndConfig.scheduled = dnd["scheduled"];
    }
    if (dnd["startHour"].is<int>()) {
      _dndConfig.startHour = dnd["startHour"];
    }
    if (dnd["startMinute"].is<int>()) {
      _dndConfig.startMinute = dnd["startMinute"];
    }
    if (dnd["endHour"].is<int>()) {
      _dndConfig.endHour = dnd["endHour"];
    }
    if (dnd["endMinute"].is<int>()) {
      _dndConfig.endMinute = dnd["endMinute"];
    }
  }

  // Quick dial entries
  if (doc["quickDial"]) {
    _quickDialEntries.clear();
    JsonObject quickDial = doc["quickDial"];
    for (JsonPair entry : quickDial) {
      _quickDialEntries[entry.key().c_str()] = entry.value().as<String>();
    }
    Logger::infoln(F("Loaded %d quick dial entries"), _quickDialEntries.size());
  }

  // Blocked numbers
  if (doc["blockedNumbers"]) {
    _blockedNumbers.clear();
    JsonArray blocked = doc["blockedNumbers"];
    for (JsonVariant number : blocked) {
      _blockedNumbers.push_back(number.as<String>());
    }
    Logger::infoln(F("Loaded %d blocked numbers"), _blockedNumbers.size());
  }

  // Webhook actions
  if (doc["webhookActions"]) {
    _webhookActions.clear();
    JsonObject webhooks = doc["webhookActions"];
    for (JsonPair action : webhooks) {
      _webhookActions[action.key().c_str()] = action.value().as<String>();
    }
    Logger::infoln(F("Loaded %d webhook actions"), _webhookActions.size());
  }

  // Ring pattern
  if (doc["ringPattern"].is<const char *>()) {
    _ringPattern = doc["ringPattern"].as<String>();
  }

  // Home Assistant URL
  if (doc["homeAssistantUrl"].is<const char *>()) {
    _homeAssistantUrl = doc["homeAssistantUrl"].as<String>();
  }

  return true;
}

void DeviceConfig::initializeDefaults() {
  // Initialize with generated phoneBook entries dynamically
  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; ++i) {
    _quickDialEntries[String(phoneBookEntries[i].entry)] = String(phoneBookEntries[i].number);
  }

  // Initialize audio config with defaults from config.h
  _audioConfig.earpieceVolume = kEarpieceVolume;
  _audioConfig.earpieceGain = kEarpieceMicGain;
  _audioConfig.speakerVolume = kSpeakerVolume;
  _audioConfig.speakerGain = kSpeakerMicGain;

  // Initialize DND config with defaults from config.h
  _dndConfig.force = kDndForce;
  _dndConfig.scheduled = kDndScheduled;
  _dndConfig.startHour = kDndStartHour;
  _dndConfig.startMinute = kDndStartMinute;
  _dndConfig.endHour = kDndEndHour;
  _dndConfig.endMinute = kDndEndMinute;

  _ringPattern = kDefaultRingPattern;
  _homeAssistantUrl = "http://homeassistant.local:8123";
}

void DeviceConfig::setAudioConfig(const AudioConfig &config) {
  _audioConfig = config;
  saveAndNotify(ConfigChangeType::Audio);
}

void DeviceConfig::setDndConfig(const DndConfig &config) {
  _dndConfig = config;
  saveAndNotify(ConfigChangeType::DND);
}

bool DeviceConfig::addQuickDialEntry(const String &code, const String &number) {
  if (isCodeConflict(code)) {
    return false;
  }

  _quickDialEntries[code] = number;
  saveAndNotify(ConfigChangeType::QuickDial);
  return true;
}

bool DeviceConfig::removeQuickDialEntry(const String &code) {
  auto it = _quickDialEntries.find(code);
  if (it != _quickDialEntries.end()) {
    _quickDialEntries.erase(it);
    saveAndNotify(ConfigChangeType::QuickDial);
    return true;
  }
  return false;
}

String DeviceConfig::getQuickDialNumber(const String &code) const {
  auto it = _quickDialEntries.find(code);
  return (it != _quickDialEntries.end()) ? it->second : String();
}

bool DeviceConfig::hasQuickDialEntry(const String &code) const {
  return _quickDialEntries.find(code) != _quickDialEntries.end();
}

bool DeviceConfig::addBlockedNumber(const String &number) {
  for (const String &blocked : _blockedNumbers) {
    if (blocked == number) {
      return false;
    }
  }

  _blockedNumbers.push_back(number);
  saveAndNotify(ConfigChangeType::BlockedNumbers);
  return true;
}

bool DeviceConfig::removeBlockedNumber(const String &number) {
  auto it = std::find(_blockedNumbers.begin(), _blockedNumbers.end(), number);
  if (it != _blockedNumbers.end()) {
    _blockedNumbers.erase(it);
    saveAndNotify(ConfigChangeType::BlockedNumbers);
    return true;
  }
  return false;
}

bool DeviceConfig::isIncomingCallBlocked(const String &number) const {
  return std::find(_blockedNumbers.begin(), _blockedNumbers.end(), number) != _blockedNumbers.end();
}

bool DeviceConfig::addWebhookAction(const String &code, const String &webhookId) {
  if (isCodeConflict(code)) {
    return false;
  }

  _webhookActions[code] = webhookId;
  saveAndNotify(ConfigChangeType::WebhookActions);
  return true;
}

bool DeviceConfig::removeWebhookAction(const String &code) {
  auto it = _webhookActions.find(code);
  if (it != _webhookActions.end()) {
    _webhookActions.erase(it);
    saveAndNotify(ConfigChangeType::WebhookActions);
    return true;
  }
  return false;
}

String DeviceConfig::getWebhookId(const String &code) const {
  auto it = _webhookActions.find(code);
  return (it != _webhookActions.end()) ? it->second : String();
}

bool DeviceConfig::hasWebhookAction(const String &code) const {
  return _webhookActions.find(code) != _webhookActions.end();
}

void DeviceConfig::setRingPattern(const String &pattern) {
  if (_ringPattern != pattern) {
    _ringPattern = pattern;
    saveAndNotify(ConfigChangeType::RingPattern);
  }
}

void DeviceConfig::incrementResetCount() {
  _resetCount++;
  save();
}

void DeviceConfig::notifyConfigChanged(ConfigChangeType changeType) {
  if (_configChangeCallback) {
    _configChangeCallback(changeType);
  }
}

void DeviceConfig::saveAndNotify(ConfigChangeType changeType) {
  save();
  notifyConfigChanged(changeType);
}

bool DeviceConfig::isCodeConflict(const String &code) const {
  return hasQuickDialEntry(code) || hasWebhookAction(code);
}

void DeviceConfig::setHomeAssistantUrl(const String &url) {
  if (_homeAssistantUrl != url) {
    _homeAssistantUrl = url;
    save(); // Save immediately, no need to notify since this is integration-specific
  }
}
