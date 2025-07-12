#pragma once

#include <Arduino.h>
#include <vector>

// Core phone data that syncs across all servers
struct QuickDialEntry {
  String name;
  String number;
};

struct DndSettings {
  bool force_enabled = false;    // DND always on regardless of time
  bool schedule_enabled = false; // DND only during specified hours
  uint8_t startHour = 22;
  uint8_t startMinute = 0;
  uint8_t endHour = 8;
  uint8_t endMinute = 0;
};

// Core phone configuration - shared across all servers
// On first initialization, automatically seeds quick dial entries from the local phonebook
// and DnD settings from local config, making them available to all servers (HA, Android, etc.)
class CorePhoneConfig {
public:
  CorePhoneConfig();

  // Initialization
  void init();

  // Configuration management
  void load();
  void save();

  // Quick dial management
  bool addQuickDial(const String &name, const String &number);
  bool removeQuickDial(const String &name);
  const char *getQuickDialNumber(const String &name) const;
  bool isQuickDialEntry(const String &number) const;
  const std::vector<QuickDialEntry> &getQuickDialEntries() const {
    return _quickDialEntries;
  }

  // New interface methods for better compatibility with HA server
  bool addQuickDialEntry(const QuickDialEntry &entry);
  bool removeQuickDialEntry(const String &name);
  QuickDialEntry getQuickDialEntry(const String &name) const;
  bool hasQuickDialEntry(const String &name) const;
  bool hasPartialQuickDialMatch(const String &partialNumber) const;
  const std::vector<QuickDialEntry> &getAllQuickDialEntries() const {
    return _quickDialEntries;
  }

  // Blocked numbers management
  bool addBlockedNumber(const String &number);
  bool removeBlockedNumber(const String &number);
  bool isNumberBlocked(const String &number) const;
  const std::vector<String> &getBlockedNumbers() const {
    return _blockedNumbers;
  }

  // DnD settings management
  void setDndForceEnabled(bool enabled);
  bool isDndForceEnabled() const {
    return _dndSettings.force_enabled;
  }
  void setDndScheduleEnabled(bool enabled);
  bool isDndScheduleEnabled() const {
    return _dndSettings.schedule_enabled;
  }
  void setDndHours(uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute);
  void getDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute) const;
  const DndSettings &getDndSettings() const {
    return _dndSettings;
  }

  // Maintenance mode management
  void setMaintenanceModeEnabled(bool enabled);
  bool isMaintenanceModeEnabled() const {
    return _maintenanceModeEnabled;
  }

  // Phonebook operations (consolidates IPhoneBookService functionality)
  bool isPhoneBookEntry(const char *number) const;
  bool isPartialOfPhoneBookEntry(const char *number) const;
  const char *getPhoneBookNumberForEntry(const char *entry) const;

private:
  std::vector<QuickDialEntry> _quickDialEntries;
  std::vector<String> _blockedNumbers;
  DndSettings _dndSettings;
  bool _maintenanceModeEnabled;
  bool _seededFromLocalConfig = false;

  void seedFromLocalConfig();
};

// Global core phone config instance
extern CorePhoneConfig *g_corePhoneConfig;

// Factory function
CorePhoneConfig *createCorePhoneConfig();
