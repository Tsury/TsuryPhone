#include "DeviceConfig.h"
#include "../common/logger.h"
#include "../config.h"
#include "../generated/phoneBook.h"
#include "../generated/priority_callers.h"
#include "../generated/blocked_numbers.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <algorithm>

const char *DeviceConfig::kConfigFilePath = "/config.json";
const char *DeviceConfig::kDefaultRingPattern = "500,500,500,500x3";

DeviceConfig::DeviceConfig() {
  _deviceId = generateDeviceId();
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

  // Priority callers
  JsonArray priority = doc["priorityCallers"].to<JsonArray>();
  for (const auto &num : _priorityCallers) {
    priority.add(num);
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

  // Priority callers
  if (doc["priorityCallers"]) {
    _priorityCallers.clear();
    JsonArray priority = doc["priorityCallers"];
    for (JsonVariant numVar : priority) {
      if (numVar.is<const char *>()) {
        _priorityCallers.push_back(String(numVar.as<const char *>()));
      }
    }
    Logger::infoln(F("Loaded %d priority callers"), _priorityCallers.size());
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

  // Seed priority callers from generated header (bootstrap only on fresh defaults)
  size_t prioCount = getPriorityCallersCount();
  for (size_t i = 0; i < prioCount; ++i) {
    if (!isBlockedNumber(priorityCallerNumbers[i])) { // defensive conflict guard
      _priorityCallers.push_back(String(priorityCallerNumbers[i]));
    }
  }

  // Seed blocked numbers from generated header (bootstrap only on fresh defaults)
  size_t blockedCount = getBlockedNumbersCount();
  for (size_t i = 0; i < blockedCount; ++i) {
    const char *bn = blockedNumbers[i];
    if (bn && *bn) {
      // Priority list takes precedence; do not add if seeded as priority (policy: no overlap)
      if (!isPriorityCaller(String(bn))) {
        _blockedNumbers.push_back(BlockedNumberEntry(String(bn), String("seed")));
      }
    }
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
  // Prevent adding a blocked number that is already a priority caller (policy: cannot conflict)
  if (isPriorityCaller(number)) {
    Logger::warnln(F("Cannot block number %s: it is a priority caller"), number.c_str());
    return false;
  }
  auto it =
      std::find_if(_blockedNumbers.begin(),
                   _blockedNumbers.end(),
                   [&number](const BlockedNumberEntry &entry) { return entry.number == number; });
  if (it != _blockedNumbers.end()) {
    Logger::debugln(F("Blocked number already present: %s"), number.c_str());
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

bool DeviceConfig::addPriorityCaller(const String &number) {
  // Cannot add if currently blocked
  if (isIncomingCallBlocked(number)) {
    Logger::warnln(F("Cannot add priority caller %s: number is blocked"), number.c_str());
    return false;
  }
  auto it = std::find_if(_priorityCallers.begin(), _priorityCallers.end(), [&number](const String &n) { return n == number; });
  if (it != _priorityCallers.end()) {
    Logger::debugln(F("Priority caller already present: %s"), number.c_str());
    return false; // already present
  }
  _priorityCallers.push_back(number);
  saveAndNotify(ConfigChangeType::PriorityCallers);
  return true;
}

bool DeviceConfig::removePriorityCaller(const String &number) {
  auto it = std::find_if(_priorityCallers.begin(), _priorityCallers.end(), [&number](const String &n) { return n == number; });
  if (it != _priorityCallers.end()) {
    _priorityCallers.erase(it);
    saveAndNotify(ConfigChangeType::PriorityCallers);
    return true;
  }
  return false;
}

bool DeviceConfig::isPriorityCaller(const String &number) const {
  auto it = std::find_if(_priorityCallers.begin(), _priorityCallers.end(), [&number](const String &n) { return n == number; });
  return it != _priorityCallers.end();
}

DeviceConfig::NumberClassification DeviceConfig::classifyNumber(const String &number) const {
  NumberClassification c;
  if (number.isEmpty()) {
    return c; // defaults false
  }
  c.isBlocked = isIncomingCallBlocked(number);
  c.isPriority = isPriorityCaller(number);
  // Policy: blocked supersedes priority logically for ring suppression, but we expose both flags
  return c;
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
  return hasQuickDialEntry(code);
}

void DeviceConfig::setHomeAssistantUrl(const String &url) {
  if (_homeAssistantUrl != url) {
    _homeAssistantUrl = url;
    save(); // Save immediately, no need to notify since this is integration-specific
  }
}
