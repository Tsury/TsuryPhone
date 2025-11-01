#pragma once

#include <Arduino.h>
#include <functional>
#include <map>
#include <vector>

// Enum to specify what configuration changed
enum class ConfigChangeType {
  Audio,
  DND,
  QuickDial,
  PriorityCallers,
  BlockedNumbers,
  RingPattern,
  DeviceName,
  DefaultDialingCode
};

struct AudioConfig {
  int earpieceVolume = 2;
  int earpieceGain = 7;
  int speakerVolume = 7;
  int speakerGain = 7;
};

struct DndConfig {
  bool force = false;
  bool scheduled = true;
  int startHour = 18;
  int startMinute = 30;
  int endHour = 8;
  int endMinute = 30;
};

struct QuickDialEntry {
  String id;                // Unique identifier (generated)
  String code;              // Optional quick dial code (can be empty)
  String number;            // Normalized E.164 format (e.g., "+972546662771")
  String name;

  QuickDialEntry() = default;
  QuickDialEntry(const String &i,
                 const String &c,
                 const String &n,
                 const String &nm = "")
      : id(i), code(c), number(n), name(nm) {}
};

struct BlockedNumberEntry {
  String id;                // Unique identifier (generated)
  String number;            // Normalized E.164 format
  String name;

  BlockedNumberEntry() = default;
  BlockedNumberEntry(const String &i, const String &n, const String &nm = "")
      : id(i), number(n), name(nm) {}
};

struct PriorityCallerEntry {
  String id;                // Unique identifier (generated)
  String number;            // Normalized E.164 format

  PriorityCallerEntry() = default;
  PriorityCallerEntry(const String &i, const String &n)
      : id(i), number(n) {}
};

class DeviceConfig {
public:
  DeviceConfig();

  bool init();
  bool save();
  bool load();

  bool resetToFactoryDefaults();

  // Device identification

  String getDeviceId() const {
    return _deviceId;
  }

  String getWifiSsid() const {
    return _deviceId;
  }

  // Audio configuration
  const AudioConfig &getAudioConfig() const {
    return _audioConfig;
  }
  void setAudioConfig(const AudioConfig &config);

  // DND configuration
  const DndConfig &getDndConfig() const {
    return _dndConfig;
  }
  void setDndConfig(const DndConfig &config);

  // Quick dial entries
  const std::vector<QuickDialEntry> &getQuickDialEntries() const {
    return _quickDialEntries;
  }
  bool addQuickDialEntry(const String &code, const String &number, const String &name = "");
  bool removeQuickDialById(const String &id);
  String getQuickDialNumber(const String &code) const;
  QuickDialEntry* getQuickDialById(const String &id);
  const QuickDialEntry* getQuickDialById(const String &id) const;
  bool hasQuickDialEntry(const String &code) const;
  bool hasQuickDialId(const String &id) const;

  // Blocked numbers
  const std::vector<BlockedNumberEntry> &getBlockedNumbers() const {
    return _blockedNumbers;
  }
  bool addBlockedNumber(const String &number, const String &name = "");
  bool removeBlockedNumberById(const String &id);
  BlockedNumberEntry* getBlockedNumberById(const String &id);
  const BlockedNumberEntry* getBlockedNumberById(const String &id) const;
  bool isIncomingCallBlocked(const String &number) const;

  // Priority callers
  const std::vector<PriorityCallerEntry> &getPriorityCallers() const {
    return _priorityCallers;
  }
  bool addPriorityCaller(const String &number);
  bool removePriorityCallerById(const String &id);
  PriorityCallerEntry* getPriorityCallerById(const String &id);
  const PriorityCallerEntry* getPriorityCallerById(const String &id) const;
  bool isPriorityCaller(const String &number) const;

  // Default dialing code
  String getDefaultDialingCode() const {
    return _defaultDialingCode;
  }
  void setDefaultDialingCode(const String &code);
  String normalizeNumber(const String &number) const;

  // Ring pattern
  String getRingPattern() const {
    return _ringPattern;
  }
  void setRingPattern(const String &pattern);

  static const char *getDefaultRingPattern() {
    return kDefaultRingPattern;
  }

  // Home Assistant URL
  String getHomeAssistantUrl() const {
    return _homeAssistantUrl;
  }
  void setHomeAssistantUrl(const String &url);

  // Configuration change callbacks
  void setConfigChangeCallback(std::function<void(ConfigChangeType)> callback) {
    _configChangeCallback = callback;
  }

private:
  void initializeDefaults();
  bool isCodeConflict(const String &code) const;
  String generateQuickDialId();
  String generateBlockedNumberId();
  String generatePriorityCallerId();
  void notifyConfigChanged(ConfigChangeType changeType);
  void saveAndNotify(ConfigChangeType changeType);
  void refreshNormalizedNumbers();
  bool updateNormalizedNumber(String &number, String &normalized) const;
  static String sanitizeDefaultDialingCodeSeed(const String &seed);
  static String sanitizeDefaultDialingCodeSeed(const char *seed);

  String _deviceId;
  AudioConfig _audioConfig;
  DndConfig _dndConfig;
  std::vector<QuickDialEntry> _quickDialEntries;
  std::vector<BlockedNumberEntry> _blockedNumbers;
  std::vector<PriorityCallerEntry> _priorityCallers; // numbers that bypass DND / special handling
  String _ringPattern;
  String _homeAssistantUrl;
  String _defaultDialingCode;

  std::function<void(ConfigChangeType)> _configChangeCallback;

  static const char *kConfigFilePath;
  static const char *kDefaultRingPattern;
  static const char *kDefaultDialingCodeValue;
};
