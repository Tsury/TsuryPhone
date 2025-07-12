#include "../config/corePhoneConfig.h"
#include "../generated/phoneBook.h"
#include "../utils/logger.h"
#include "config.h"

// Conditional includes - only when persistence is available
#ifdef HOME_ASSISTANT_INTEGRATION
#include <ArduinoJson.h>
#include <SPIFFS.h>
#define PERSISTENCE_AVAILABLE 1
#else
#define PERSISTENCE_AVAILABLE 0
#endif

namespace {
  const constexpr char *kCoreConfigFile = "/phone_config.json";
}

// Global core phone config instance
CorePhoneConfig *g_corePhoneConfig = nullptr;

CorePhoneConfig::CorePhoneConfig() {
  // Initialize with default DnD settings from config
  _dndSettings.force_enabled = false;
  _dndSettings.schedule_enabled = kDndEnabled;
  _dndSettings.startHour = kDndStartHour;
  _dndSettings.startMinute = kDndStartMinute;
  _dndSettings.endHour = kDndEndHour;
  _dndSettings.endMinute = kDndEndMinute;
  _maintenanceModeEnabled = false;
  _seededFromLocalConfig = false;
}

void CorePhoneConfig::init() {
  Logger::infoln(F("Initializing core phone config..."));
  load();
}

void CorePhoneConfig::load() {
#if PERSISTENCE_AVAILABLE
  if (!SPIFFS.exists(kCoreConfigFile)) {
    // On first run, seed quick dial entries from local phonebook and DnD settings
    // This makes the local config available to all servers
    seedFromLocalConfig();
    Logger::infoln(F("First run: seeded core config with local config"));
    return;
  }

  File file = SPIFFS.open(kCoreConfigFile, "r");
  if (!file) {
    Logger::errorln(F("Failed to open core config file"));
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    Logger::errorln(F("Failed to parse core config JSON"));
    file.close();
    return;
  }

  // Load quick dial entries
  if (doc["quick_dial"].is<JsonArray>()) {
    _quickDialEntries.clear();
    for (JsonObject entry : doc["quick_dial"].as<JsonArray>()) {
      QuickDialEntry qd;
      qd.name = entry["name"].as<String>();
      qd.number = entry["number"].as<String>();
      _quickDialEntries.push_back(qd);
    }
  }

  // Load blocked numbers
  if (doc["blocked_numbers"].is<JsonArray>()) {
    _blockedNumbers.clear();
    for (const String &number : doc["blocked_numbers"].as<JsonArray>()) {
      _blockedNumbers.push_back(number);
    }
  }

  // Load DnD settings
  if (doc["dnd"].is<JsonObject>()) {
    JsonObject dnd = doc["dnd"];
    _dndSettings.force_enabled = dnd["force_enabled"] | false;
    _dndSettings.schedule_enabled = dnd["schedule_enabled"] | kDndEnabled;
    _dndSettings.startHour = dnd["start_hour"] | kDndStartHour;
    _dndSettings.startMinute = dnd["start_minute"] | kDndStartMinute;
    _dndSettings.endHour = dnd["end_hour"] | kDndEndHour;
    _dndSettings.endMinute = dnd["end_minute"] | kDndEndMinute;
  }

  // Load seeded flag
  _seededFromLocalConfig = doc["seeded_from_local_config"] | false;

  file.close();
  Logger::infoln(F("Core phone config loaded: %d quick dials, %d blocked numbers"),
                 _quickDialEntries.size(),
                 _blockedNumbers.size());
#else
  // When no persistence, seed from local config on first run
  // This makes the local phonebook and DnD settings available to all servers
  if (!_seededFromLocalConfig) {
    seedFromLocalConfig();
    _seededFromLocalConfig = true;
    Logger::infoln(F("First run (no persistence): seeded core config with local config"));
  } else {
    Logger::infoln(
        F("Core phone config initialized (no persistence): %d quick dials, %d blocked numbers"),
        _quickDialEntries.size(),
        _blockedNumbers.size());
  }
#endif
}

void CorePhoneConfig::save() {
#if PERSISTENCE_AVAILABLE
  JsonDocument doc;

  // Save quick dial entries
  JsonArray quickDial = doc["quick_dial"].to<JsonArray>();
  for (const auto &entry : _quickDialEntries) {
    JsonObject entryObj = quickDial.add<JsonObject>();
    entryObj["name"] = entry.name;
    entryObj["number"] = entry.number;
  }

  // Save blocked numbers
  JsonArray blocked = doc["blocked_numbers"].to<JsonArray>();
  for (const String &number : _blockedNumbers) {
    blocked.add(number);
  }

  // Save DnD settings
  JsonObject dnd = doc["dnd"].to<JsonObject>();
  dnd["force_enabled"] = _dndSettings.force_enabled;
  dnd["schedule_enabled"] = _dndSettings.schedule_enabled;
  dnd["start_hour"] = _dndSettings.startHour;
  dnd["start_minute"] = _dndSettings.startMinute;
  dnd["end_hour"] = _dndSettings.endHour;
  dnd["end_minute"] = _dndSettings.endMinute;

  // Save seeded flag to prevent re-seeding on subsequent boots
  doc["seeded_from_local_config"] = _seededFromLocalConfig;

  // NOTE: Maintenance mode is NOT saved - it should never persist across reboots
  // and always starts disabled

  File file = SPIFFS.open(kCoreConfigFile, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Logger::infoln(F("Core phone config saved"));
  } else {
    Logger::errorln(F("Failed to save core phone config"));
  }
#else
  // No persistence available - settings are kept in memory only
  Logger::debugln(F("Core phone config changes kept in memory (no persistence)"));
#endif
}

bool CorePhoneConfig::addQuickDial(const String &name, const String &number) {
  // Check for existing name or number
  for (const auto &entry : _quickDialEntries) {
    if (entry.name == name || entry.number == number) {
      return false; // Already exists
    }
  }

  QuickDialEntry entry;
  entry.name = name;
  entry.number = number;
  _quickDialEntries.push_back(entry);
  save();
  return true;
}

bool CorePhoneConfig::removeQuickDial(const String &name) {
  auto it = std::find_if(_quickDialEntries.begin(),
                         _quickDialEntries.end(),
                         [&](const QuickDialEntry &entry) { return entry.name == name; });
  if (it != _quickDialEntries.end()) {
    _quickDialEntries.erase(it);
    save();
    return true;
  }
  return false;
}

const char *CorePhoneConfig::getQuickDialNumber(const String &name) const {
  static String cachedResult;
  for (const auto &entry : _quickDialEntries) {
    if (entry.name == name) {
      cachedResult = entry.number;
      return cachedResult.c_str();
    }
  }
  return nullptr;
}

bool CorePhoneConfig::isQuickDialEntry(const String &number) const {
  for (const auto &entry : _quickDialEntries) {
    if (entry.number == number) {
      return true;
    }
  }
  return false;
}

bool CorePhoneConfig::addBlockedNumber(const String &number) {
  if (std::find(_blockedNumbers.begin(), _blockedNumbers.end(), number) == _blockedNumbers.end()) {
    _blockedNumbers.push_back(number);
    save();
    return true;
  }
  return false;
}

bool CorePhoneConfig::removeBlockedNumber(const String &number) {
  auto it = std::find(_blockedNumbers.begin(), _blockedNumbers.end(), number);
  if (it != _blockedNumbers.end()) {
    _blockedNumbers.erase(it);
    save();
    return true;
  }
  return false;
}

bool CorePhoneConfig::isNumberBlocked(const String &number) const {
  return std::find(_blockedNumbers.begin(), _blockedNumbers.end(), number) != _blockedNumbers.end();
}

void CorePhoneConfig::setDndForceEnabled(bool enabled) {
  _dndSettings.force_enabled = enabled;
  save();
}

void CorePhoneConfig::setDndScheduleEnabled(bool enabled) {
  _dndSettings.schedule_enabled = enabled;
  save();
}

void CorePhoneConfig::setDndHours(uint8_t startHour,
                                  uint8_t startMinute,
                                  uint8_t endHour,
                                  uint8_t endMinute) {
  _dndSettings.startHour = startHour;
  _dndSettings.startMinute = startMinute;
  _dndSettings.endHour = endHour;
  _dndSettings.endMinute = endMinute;
  save();
}

void CorePhoneConfig::getDndHours(int &startHour,
                                  int &startMinute,
                                  int &endHour,
                                  int &endMinute) const {
  startHour = _dndSettings.startHour;
  startMinute = _dndSettings.startMinute;
  endHour = _dndSettings.endHour;
  endMinute = _dndSettings.endMinute;
}

// Maintenance mode management
void CorePhoneConfig::setMaintenanceModeEnabled(bool enabled) {
  _maintenanceModeEnabled = enabled;
  save();
}

void CorePhoneConfig::seedFromLocalConfig() {
  // Seed quick dial entries from the generated local phonebook
  // This makes the local phonebook available to all servers through CorePhoneConfig
  _quickDialEntries.clear();
  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; i++) {
    QuickDialEntry entry;
    entry.name = phoneBookEntries[i].entry;
    entry.number = phoneBookEntries[i].number;
    _quickDialEntries.push_back(entry);
  }

  // Seed DnD settings from local config
  // This ensures the local config DnD settings are explicitly saved on first run
  _dndSettings.force_enabled = false;
  _dndSettings.schedule_enabled = kDndEnabled;
  _dndSettings.startHour = kDndStartHour;
  _dndSettings.startMinute = kDndStartMinute;
  _dndSettings.endHour = kDndEndHour;
  _dndSettings.endMinute = kDndEndMinute;

  _seededFromLocalConfig = true;
  Logger::infoln(F("Seeded core config with %d local phonebook entries and DnD settings"),
                 numEntries);
  save(); // Persist the seeded data if persistence is available
}

CorePhoneConfig *createCorePhoneConfig() {
  return new CorePhoneConfig();
}

// New interface methods for better compatibility with HA server
bool CorePhoneConfig::addQuickDialEntry(const QuickDialEntry &entry) {
  return addQuickDial(entry.name, entry.number);
}

bool CorePhoneConfig::removeQuickDialEntry(const String &name) {
  return removeQuickDial(name);
}

QuickDialEntry CorePhoneConfig::getQuickDialEntry(const String &name) const {
  QuickDialEntry result;
  result.name = "";
  result.number = "";

  for (const auto &entry : _quickDialEntries) {
    if (entry.name == name) {
      return entry;
    }
  }
  return result;
}

bool CorePhoneConfig::hasQuickDialEntry(const String &name) const {
  for (const auto &entry : _quickDialEntries) {
    if (entry.name == name) {
      return true;
    }
  }
  return false;
}

bool CorePhoneConfig::hasPartialQuickDialMatch(const String &partialNumber) const {
  for (const auto &entry : _quickDialEntries) {
    if (String(entry.number).startsWith(partialNumber)) {
      return true;
    }
  }
  return false;
}

// Phonebook operations (consolidates IPhoneBookService functionality)
bool CorePhoneConfig::isPhoneBookEntry(const char *number) const {
  // Check quick dial entries first
  if (isQuickDialEntry(String(number))) {
    return true;
  }

  // Check generated phonebook
  return ::isPhoneBookEntry(number);
}

bool CorePhoneConfig::isPartialOfPhoneBookEntry(const char *number) const {
  // Check quick dial entries for partial matches
  if (hasPartialQuickDialMatch(String(number))) {
    return true;
  }

  // Check generated phonebook
  return ::isPartialOfFullPhoneBookEntry(number);
}

const char *CorePhoneConfig::getPhoneBookNumberForEntry(const char *entry) const {
  // Check quick dial entries first
  const char *number = getQuickDialNumber(String(entry));
  if (number) {
    return number;
  }

  // Check generated phonebook
  return ::getPhoneBookNumberForEntry(entry);
}
