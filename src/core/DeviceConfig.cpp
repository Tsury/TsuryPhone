#include "DeviceConfig.h"
#include "../common/logger.h"
#include "../common/phoneNormalization.h"
#include "../config.h"
#include "../generated/blocked_numbers.h"
#include "../generated/phoneBook.h"
#include "../generated/priority_callers.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <algorithm>

const char *DeviceConfig::kConfigFilePath = "/config.json";
const char *DeviceConfig::kDefaultRingPattern = "2000";
const char *DeviceConfig::kDefaultDialingCodeValue = kPhoneBookDefaultDialingCode;

DeviceConfig::DeviceConfig() {
  _deviceId = generateDeviceId();
  _defaultDialingCode = sanitizeDefaultDialingCodeSeed(kDefaultDialingCodeValue);
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
  audio["ringerCycleDuration"] = _audioConfig.ringerCycleDuration;

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
    entryObj["id"] = entry.id;
    entryObj["code"] = entry.code;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
  }

  // Blocked numbers
  JsonArray blocked = doc["blockedNumbers"].to<JsonArray>();
  for (const auto &entry : _blockedNumbers) {
    JsonObject entryObj = blocked.add<JsonObject>();
    entryObj["id"] = entry.id;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
  }

  // Priority callers
  JsonArray priority = doc["priorityCallerDetails"].to<JsonArray>();
  for (const auto &entry : _priorityCallers) {
    JsonObject obj = priority.add<JsonObject>();
    obj["id"] = entry.id;
    obj["number"] = entry.number;
  }

  // Ring pattern
  doc["ringPattern"] = _ringPattern;

  // Home Assistant URL
  doc["homeAssistantUrl"] = _homeAssistantUrl;

  // Default dialing code (normalized digits only)
  doc["defaultDialingCode"] = _defaultDialingCode;

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
    if (audio["ringerCycleDuration"].is<int>()) {
      _audioConfig.ringerCycleDuration = audio["ringerCycleDuration"];
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

  if (doc["defaultDialingCode"].is<const char *>()) {
    _defaultDialingCode = sanitizeDefaultDialingCodeSeed(doc["defaultDialingCode"].as<String>());
  } else if (_defaultDialingCode.isEmpty()) {
    _defaultDialingCode = sanitizeDefaultDialingCodeSeed(kDefaultDialingCodeValue);
  }

  // Quick dial entries
  if (doc["quickDial"]) {
    _quickDialEntries.clear();
    JsonArray quickDial = doc["quickDial"];
    for (JsonVariant entry : quickDial) {
      if (entry.is<JsonObject>()) {
        JsonObject entryObj = entry.as<JsonObject>();
        QuickDialEntry qde;
        qde.id = entryObj["id"].as<String>();
        qde.code = entryObj["code"].as<String>();
        qde.number = entryObj["number"].as<String>();
        qde.name = entryObj["name"].as<String>();
        updateNormalizedNumber(qde.number, qde.number); // Normalize in-place
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
        bne.id = entryObj["id"].as<String>();
        bne.number = entryObj["number"].as<String>();
        if (entryObj["name"].is<const char *>()) {
          bne.name = entryObj["name"].as<String>();
        }
        updateNormalizedNumber(bne.number, bne.number); // Normalize in-place
        _blockedNumbers.push_back(bne);
      }
    }
    Logger::infoln(F("Loaded %d blocked numbers"), _blockedNumbers.size());
  }

  // Priority callers
  if (doc["priorityCallerDetails"]) {
    _priorityCallers.clear();
    JsonArray priority = doc["priorityCallerDetails"];
    for (JsonVariant entryVar : priority) {
      if (!entryVar.is<JsonObject>()) {
        continue;
      }
      JsonObject obj = entryVar.as<JsonObject>();
      if (!obj["number"].is<const char *>()) {
        continue;
      }

      PriorityCallerEntry entry;
      entry.id = obj["id"].as<String>();
      entry.number = obj["number"].as<String>();
      updateNormalizedNumber(entry.number, entry.number); // Normalize in-place
      if (!entry.number.isEmpty()) {
        _priorityCallers.push_back(entry);
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

  refreshNormalizedNumbers();

  return true;
}

bool DeviceConfig::resetToFactoryDefaults() {
  Logger::warnln(F("DeviceConfig: factory reset requested"));

  bool removed = true;
  if (SPIFFS.exists(kConfigFilePath)) {
    if (!SPIFFS.remove(kConfigFilePath)) {
      Logger::errorln(F("Failed to remove config file during factory reset"));
      removed = false;
    }
  }

  _quickDialEntries.clear();
  _blockedNumbers.clear();
  _priorityCallers.clear();

  _audioConfig = AudioConfig();
  _dndConfig = DndConfig();
  _ringPattern = "";
  _homeAssistantUrl = "";
  _defaultDialingCode = sanitizeDefaultDialingCodeSeed(kDefaultDialingCodeValue);

  initializeDefaults();

  return removed;
}

void DeviceConfig::initializeDefaults() {
  // Initialize with generated phoneBook entries dynamically
  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; ++i) {
    const PhoneBookEntry &seed = phoneBookEntries[i];
    String id = generateQuickDialId();
    String number = String(seed.number);
    updateNormalizedNumber(number, number); // Normalize in-place
    QuickDialEntry entry{id, String(seed.entry), number, String(seed.name)};
    _quickDialEntries.push_back(entry);
  }

  // Seed priority callers from generated header (bootstrap only on fresh defaults)
  size_t prioCount = getPriorityCallersCount();
  for (size_t i = 0; i < prioCount; ++i) {
    if (!isBlockedNumber(priorityCallerNumbers[i])) { // defensive conflict guard
      String id = generatePriorityCallerId();
      String number = String(priorityCallerNumbers[i]);
      updateNormalizedNumber(number, number); // Normalize in-place
      PriorityCallerEntry entry{id, number};
      _priorityCallers.push_back(entry);
    }
  }

  // Seed blocked numbers from generated header (bootstrap only on fresh defaults)
  size_t blockedCount = getBlockedNumbersCount();
  for (size_t i = 0; i < blockedCount; ++i) {
    const char *bn = blockedNumbers[i];
    if (bn && *bn) {
      // Priority list takes precedence; do not add if seeded as priority (policy: no overlap)
      if (!isPriorityCaller(String(bn))) {
        String id = generateBlockedNumberId();
        String number = String(bn);
        updateNormalizedNumber(number, number); // Normalize in-place
        BlockedNumberEntry entry{id, number, String("seed")};
        _blockedNumbers.push_back(entry);
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
  _defaultDialingCode = sanitizeDefaultDialingCodeSeed(kDefaultDialingCodeValue);
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
  // Allow empty code - it's now optional
  if (!code.isEmpty() && isCodeConflict(code)) {
    return false;
  }

  String trimmedNumber = number;
  trimmedNumber.trim();
  String trimmedName = name;
  trimmedName.trim();

  String normalizedNumber = trimmedNumber;
  if (!updateNormalizedNumber(normalizedNumber, normalizedNumber)) {
    Logger::warnln(F("Cannot add quick dial entry %s: normalization failed"),
                   trimmedNumber.c_str());
    return false;
  }

  // Generate unique ID
  String id = generateQuickDialId();
  QuickDialEntry entry(id, code, normalizedNumber, trimmedName);
  _quickDialEntries.push_back(entry);
  saveAndNotify(ConfigChangeType::QuickDial);
  return true;
}

bool DeviceConfig::removeQuickDialById(const String &id) {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&id](const QuickDialEntry &entry) { return entry.id == id; });
  if (it != _quickDialEntries.end()) {
    _quickDialEntries.erase(it);
    saveAndNotify(ConfigChangeType::QuickDial);
    return true;
  }
  return false;
}

QuickDialEntry *DeviceConfig::getQuickDialById(const String &id) {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&id](const QuickDialEntry &entry) { return entry.id == id; });
  if (it == _quickDialEntries.end()) {
    return nullptr;
  }
  return &(*it);
}

const QuickDialEntry *DeviceConfig::getQuickDialById(const String &id) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&id](const QuickDialEntry &entry) { return entry.id == id; });
  if (it == _quickDialEntries.end()) {
    return nullptr;
  }
  return &(*it);
}

bool DeviceConfig::hasQuickDialId(const String &id) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&id](const QuickDialEntry &entry) { return entry.id == id; });
  return it != _quickDialEntries.end();
}

String DeviceConfig::getQuickDialNumber(const String &code) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });
  if (it == _quickDialEntries.end()) {
    return String();
  }
  return it->number;
}

bool DeviceConfig::hasQuickDialEntry(const String &code) const {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&code](const QuickDialEntry &entry) { return entry.code == code; });
  return it != _quickDialEntries.end();
}

bool DeviceConfig::addBlockedNumber(const String &number, const String &name) {
  // Prevent adding a blocked number that is already a priority caller (policy: cannot conflict)
  if (isPriorityCaller(number)) {
    Logger::warnln(F("Cannot block number %s: it is a priority caller"), number.c_str());
    return false;
  }
  String trimmedNumber = number;
  trimmedNumber.trim();
  const String normalizedCandidate = normalizeNumber(trimmedNumber);
  if (normalizedCandidate.isEmpty()) {
    Logger::warnln(F("Cannot block number %s: normalization failed"), number.c_str());
    return false;
  }

  bool hasQuickDialNumber = std::any_of(
      _quickDialEntries.begin(), _quickDialEntries.end(), [&](const QuickDialEntry &entry) {
        return entry.number.equalsIgnoreCase(normalizedCandidate);
      });
  if (hasQuickDialNumber) {
    Logger::warnln(F("Cannot block number %s: it is assigned to a quick dial entry"),
                   number.c_str());
    return false;
  }
  auto it = std::find_if(
      _blockedNumbers.begin(), _blockedNumbers.end(), [&](const BlockedNumberEntry &entry) {
        return entry.number.equalsIgnoreCase(normalizedCandidate);
      });
  if (it != _blockedNumbers.end()) {
    Logger::debugln(F("Blocked number already present: %s"), number.c_str());
    return false;
  }

  String trimmedName = name;
  trimmedName.trim();

  String id = generateBlockedNumberId();
  BlockedNumberEntry entry(id, normalizedCandidate, trimmedName);
  _blockedNumbers.push_back(entry);
  saveAndNotify(ConfigChangeType::BlockedNumbers);
  return true;
}

bool DeviceConfig::removeBlockedNumberById(const String &id) {
  auto it = std::find_if(_blockedNumbers.begin(),
                         _blockedNumbers.end(),
                         [&id](const BlockedNumberEntry &entry) { return entry.id == id; });
  if (it != _blockedNumbers.end()) {
    _blockedNumbers.erase(it);
    saveAndNotify(ConfigChangeType::BlockedNumbers);
    return true;
  }
  return false;
}

BlockedNumberEntry *DeviceConfig::getBlockedNumberById(const String &id) {
  auto it = std::find_if(_blockedNumbers.begin(),
                         _blockedNumbers.end(),
                         [&id](const BlockedNumberEntry &entry) { return entry.id == id; });
  if (it == _blockedNumbers.end()) {
    return nullptr;
  }
  return &(*it);
}

const BlockedNumberEntry *DeviceConfig::getBlockedNumberById(const String &id) const {
  auto it = std::find_if(_blockedNumbers.begin(),
                         _blockedNumbers.end(),
                         [&id](const BlockedNumberEntry &entry) { return entry.id == id; });
  if (it == _blockedNumbers.end()) {
    return nullptr;
  }
  return &(*it);
}

bool DeviceConfig::isIncomingCallBlocked(const String &number) const {
  String trimmedNumber = number;
  trimmedNumber.trim();
  const String normalizedCandidate = normalizeNumber(trimmedNumber);
  if (normalizedCandidate.isEmpty()) {
    return false;
  }
  auto it = std::find_if(
      _blockedNumbers.begin(), _blockedNumbers.end(), [&](const BlockedNumberEntry &entry) {
        return entry.number.equalsIgnoreCase(normalizedCandidate);
      });
  return it != _blockedNumbers.end();
}

bool DeviceConfig::addPriorityCaller(const String &number) {
  // Cannot add if currently blocked
  if (isIncomingCallBlocked(number)) {
    Logger::warnln(F("Cannot add priority caller %s: number is blocked"), number.c_str());
    return false;
  }
  String trimmedNumber = number;
  trimmedNumber.trim();
  const String normalizedNumber = normalizeNumber(trimmedNumber);
  if (normalizedNumber.isEmpty()) {
    Logger::warnln(F("Cannot add priority caller %s: normalization failed"), number.c_str());
    return false;
  }
  auto it = std::find_if(
      _priorityCallers.begin(), _priorityCallers.end(), [&](const PriorityCallerEntry &entry) {
        return entry.number.equalsIgnoreCase(normalizedNumber);
      });
  if (it != _priorityCallers.end()) {
    Logger::debugln(F("Priority caller already present: %s"), number.c_str());
    return false; // already present
  }
  String id = generatePriorityCallerId();
  PriorityCallerEntry entry(id, normalizedNumber);
  _priorityCallers.push_back(entry);
  saveAndNotify(ConfigChangeType::PriorityCallers);
  return true;
}

bool DeviceConfig::removePriorityCallerById(const String &id) {
  auto it = std::find_if(_priorityCallers.begin(),
                         _priorityCallers.end(),
                         [&id](const PriorityCallerEntry &entry) { return entry.id == id; });
  if (it != _priorityCallers.end()) {
    _priorityCallers.erase(it);
    saveAndNotify(ConfigChangeType::PriorityCallers);
    return true;
  }
  return false;
}

PriorityCallerEntry *DeviceConfig::getPriorityCallerById(const String &id) {
  auto it = std::find_if(_priorityCallers.begin(),
                         _priorityCallers.end(),
                         [&id](const PriorityCallerEntry &entry) { return entry.id == id; });
  if (it == _priorityCallers.end()) {
    return nullptr;
  }
  return &(*it);
}

const PriorityCallerEntry *DeviceConfig::getPriorityCallerById(const String &id) const {
  auto it = std::find_if(_priorityCallers.begin(),
                         _priorityCallers.end(),
                         [&id](const PriorityCallerEntry &entry) { return entry.id == id; });
  if (it == _priorityCallers.end()) {
    return nullptr;
  }
  return &(*it);
}

bool DeviceConfig::isPriorityCaller(const String &number) const {
  String trimmedNumber = number;
  trimmedNumber.trim();
  const String normalizedNumber = normalizeNumber(trimmedNumber);
  if (normalizedNumber.isEmpty()) {
    return false;
  }
  auto it = std::find_if(
      _priorityCallers.begin(), _priorityCallers.end(), [&](const PriorityCallerEntry &entry) {
        return entry.number.equalsIgnoreCase(normalizedNumber);
      });
  return it != _priorityCallers.end();
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

String DeviceConfig::generateQuickDialId() {
  // Generate a simple unique ID using timestamp + random number
  // Format: qd_<timestamp>_<random>
  static uint32_t counter = 0;
  counter++;

  unsigned long timestamp = millis();
  uint32_t randomVal = esp_random();

  char idBuffer[32];
  snprintf(idBuffer, sizeof(idBuffer), "qd_%lu_%08x_%u", timestamp, randomVal, counter);

  return String(idBuffer);
}

String DeviceConfig::generateBlockedNumberId() {
  // Format: bl_<timestamp>_<random>
  static uint32_t counter = 0;
  counter++;

  unsigned long timestamp = millis();
  uint32_t randomVal = esp_random();

  char idBuffer[32];
  snprintf(idBuffer, sizeof(idBuffer), "bl_%lu_%08x_%u", timestamp, randomVal, counter);

  return String(idBuffer);
}

String DeviceConfig::generatePriorityCallerId() {
  // Format: pr_%<timestamp>_<random>
  static uint32_t counter = 0;
  counter++;

  unsigned long timestamp = millis();
  uint32_t randomVal = esp_random();

  char idBuffer[32];
  snprintf(idBuffer, sizeof(idBuffer), "pr_%lu_%08x_%u", timestamp, randomVal, counter);

  return String(idBuffer);
}

void DeviceConfig::setHomeAssistantUrl(const String &url) {
  if (_homeAssistantUrl != url) {
    _homeAssistantUrl = url;
    save(); // Save immediately, no need to notify since this is integration-specific
  }
}

void DeviceConfig::setDefaultDialingCode(const String &code) {
  String sanitized = sanitizeDefaultDialingCodeSeed(code);

  if (_defaultDialingCode != sanitized) {
    _defaultDialingCode = sanitized;
    refreshNormalizedNumbers();
    saveAndNotify(ConfigChangeType::DefaultDialingCode);
  }
}

String DeviceConfig::normalizeNumber(const String &number) const {
  return PhoneNormalization::normalizePhoneNumber(number, _defaultDialingCode);
}

void DeviceConfig::refreshNormalizedNumbers() {
  for (auto &entry : _quickDialEntries) {
    updateNormalizedNumber(entry.number, entry.number);
  }
  for (auto &entry : _blockedNumbers) {
    updateNormalizedNumber(entry.number, entry.number);
  }
  for (auto &entry : _priorityCallers) {
    updateNormalizedNumber(entry.number, entry.number);
  }
}

bool DeviceConfig::updateNormalizedNumber(String &number, String &normalized) const {
  normalized = normalizeNumber(number);
  if (normalized.isEmpty()) {
    return false;
  }
  number = normalized;
  return true;
}

String DeviceConfig::sanitizeDefaultDialingCodeSeed(const String &seed) {
  String sanitized = PhoneNormalization::sanitizeDefaultDialingCode(seed);
  if (!sanitized.isEmpty()) {
    return sanitized;
  }
  String fallback =
      PhoneNormalization::sanitizeDefaultDialingCode(String(kDefaultDialingCodeValue));
  if (!fallback.isEmpty()) {
    Logger::warnln(F("Default dialing code seed empty; falling back to generated phone book code"));
    return fallback;
  }

  Logger::warnln(
      F("Default dialing code seed empty and no generated fallback; using blank default"));
  return String();
}

String DeviceConfig::sanitizeDefaultDialingCodeSeed(const char *seed) {
  return sanitizeDefaultDialingCodeSeed(String(seed ? seed : ""));
}
