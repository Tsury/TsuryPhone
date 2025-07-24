#include "DeviceConfig.h"
#include "../common/logger.h"
#include "../config.h"
#include "../generated/phoneBook.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <algorithm>

const char *DeviceConfig::kConfigFilePath = "/config.json";
const char *DeviceConfig::kDefaultRingPattern = "500,500,500,500x3";

DeviceConfig::DeviceConfig() {
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

  return true;
}

bool DeviceConfig::save() {
  JsonDocument doc;

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
  JsonArray quickDial = doc["quickDial"].to<JsonArray>();
  for (const auto &entry : _quickDialEntries) {
    JsonObject entryObj = quickDial.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
  }

  // Blocked numbers
  JsonArray blocked = doc["blockedNumbers"].to<JsonArray>();
  for (const auto &entry : _blockedNumbers) {
    JsonObject entryObj = blocked.add<JsonObject>();
    entryObj["number"] = entry.number;
    entryObj["reason"] = entry.reason;
  }

  // Webhook actions
  JsonArray webhooks = doc["webhookActions"].to<JsonArray>();
  for (const auto &entry : _webhookActions) {
    JsonObject entryObj = webhooks.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["id"] = entry.id;
    entryObj["actionName"] = entry.actionName;
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
    JsonArray quickDial = doc["quickDial"];
    for (JsonVariant entry : quickDial) {
      if (entry.is<JsonObject>()) {
        JsonObject entryObj = entry.as<JsonObject>();
        QuickDialEntry qde;
        qde.code = entryObj["code"].as<String>();
        qde.number = entryObj["number"].as<String>();
        qde.name = entryObj["name"].as<String>();
        _quickDialEntries.push_back(qde);
      }
    }
    Logger::infoln(F("Loaded %d quick dial entries"), _quickDialEntries.size());
  }

  // Blocked numbers
  if (doc["blockedNumbers"]) {
    _blockedNumbers.clear();
    JsonArray blocked = doc["blockedNumbers"];
    for (JsonVariant entry : blocked) {
      if (entry.is<JsonObject>()) {
        JsonObject entryObj = entry.as<JsonObject>();
        BlockedNumberEntry bne;
        bne.number = entryObj["number"].as<String>();
        bne.reason = entryObj["reason"].as<String>();
        _blockedNumbers.push_back(bne);
      }
    }
    Logger::infoln(F("Loaded %d blocked numbers"), _blockedNumbers.size());
  }

  // Webhook actions
  if (doc["webhookActions"]) {
    _webhookActions.clear();
    JsonArray webhooks = doc["webhookActions"];
    for (JsonVariant entry : webhooks) {
      if (entry.is<JsonObject>()) {
        JsonObject entryObj = entry.as<JsonObject>();
        WebhookActionEntry wae;
        wae.code = entryObj["code"].as<String>();
        wae.id = entryObj["id"].as<String>();
        wae.actionName = entryObj["actionName"].as<String>();
        _webhookActions.push_back(wae);
      }
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
    QuickDialEntry entry(String(phoneBookEntries[i].entry), String(phoneBookEntries[i].number), "");
    _quickDialEntries.push_back(entry);
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

bool DeviceConfig::addQuickDialEntry(const String &code, const String &number, const String &name) {
  if (isCodeConflict(code)) {
    return false;
  }

  QuickDialEntry entry(code, number, name);
  _quickDialEntries.push_back(entry);
  saveAndNotify(ConfigChangeType::QuickDial);
  return true;
}

bool DeviceConfig::removeQuickDialEntry(const String &code) {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });
  if (it != _quickDialEntries.end()) {
    _quickDialEntries.erase(it);
    saveAndNotify(ConfigChangeType::QuickDial);
    return true;
  }
  return false;
}

String DeviceConfig::getQuickDialNumber(const String &code) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });
  return (it != _quickDialEntries.end()) ? it->number : String();
}

bool DeviceConfig::hasQuickDialEntry(const String &code) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });
  return it != _quickDialEntries.end();
}

bool DeviceConfig::addBlockedNumber(const String &number, const String &reason) {
  auto it =
      std::find_if(_blockedNumbers.begin(),
                   _blockedNumbers.end(),
                   [&number](const BlockedNumberEntry &entry) { return entry.number == number; });
  if (it != _blockedNumbers.end()) {
    return false;
  }

  BlockedNumberEntry entry(number, reason);
  _blockedNumbers.push_back(entry);
  saveAndNotify(ConfigChangeType::BlockedNumbers);
  return true;
}

bool DeviceConfig::removeBlockedNumber(const String &number) {
  auto it =
      std::find_if(_blockedNumbers.begin(),
                   _blockedNumbers.end(),
                   [&number](const BlockedNumberEntry &entry) { return entry.number == number; });
  if (it != _blockedNumbers.end()) {
    _blockedNumbers.erase(it);
    saveAndNotify(ConfigChangeType::BlockedNumbers);
    return true;
  }
  return false;
}

bool DeviceConfig::isIncomingCallBlocked(const String &number) const {
  auto it =
      std::find_if(_blockedNumbers.begin(),
                   _blockedNumbers.end(),
                   [&number](const BlockedNumberEntry &entry) { return entry.number == number; });
  return it != _blockedNumbers.end();
}

bool DeviceConfig::addWebhookAction(const String &code,
                                    const String &webhookId,
                                    const String &actionName) {
  if (isCodeConflict(code)) {
    return false;
  }

  WebhookActionEntry entry(code, webhookId, actionName);
  _webhookActions.push_back(entry);
  saveAndNotify(ConfigChangeType::WebhookActions);
  return true;
}

bool DeviceConfig::removeWebhookAction(const String &code) {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  if (it != _webhookActions.end()) {
    _webhookActions.erase(it);
    saveAndNotify(ConfigChangeType::WebhookActions);
    return true;
  }
  return false;
}

String DeviceConfig::getWebhookId(const String &code) const {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  return (it != _webhookActions.end()) ? it->id : String();
}

bool DeviceConfig::hasWebhookAction(const String &code) const {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  return it != _webhookActions.end();
}

void DeviceConfig::setRingPattern(const String &pattern) {
  if (_ringPattern != pattern) {
    _ringPattern = pattern;
    saveAndNotify(ConfigChangeType::RingPattern);
  }
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
