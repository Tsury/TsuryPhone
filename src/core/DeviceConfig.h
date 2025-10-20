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
  String code;
  String number;
  String normalizedNumber;
  String name;

  QuickDialEntry() = default;
  QuickDialEntry(const String &c,
                 const String &n,
                 const String &nm = "",
                 const String &normalized = "")
      : code(c), number(n), normalizedNumber(normalized), name(nm) {}

  bool hasNormalized() const {
    return !normalizedNumber.isEmpty();
  }

  const String &effectiveNumber() const {
    return normalizedNumber.isEmpty() ? number : normalizedNumber;
  }

  bool matchesNormalized(const String &candidate) const {
    return hasNormalized() && normalizedNumber.equalsIgnoreCase(candidate);
  }
};

struct BlockedNumberEntry {
  String number;
  String normalizedNumber;
  String name;

  BlockedNumberEntry() = default;
  BlockedNumberEntry(const String &n, const String &nm = "", const String &normalized = "")
      : number(n), normalizedNumber(normalized), name(nm) {}

  bool hasNormalized() const {
    return !normalizedNumber.isEmpty();
  }

  const String &effectiveNumber() const {
    return normalizedNumber.isEmpty() ? number : normalizedNumber;
  }

  bool matchesNormalized(const String &candidate) const {
    return hasNormalized() && normalizedNumber.equalsIgnoreCase(candidate);
  }
};

struct PriorityCallerEntry {
  String number;
  String normalizedNumber;

  PriorityCallerEntry() = default;
  PriorityCallerEntry(const String &n, const String &normalized = "")
      : number(n), normalizedNumber(normalized) {}

  bool hasNormalized() const {
    return !normalizedNumber.isEmpty();
  }

  const String &effectiveNumber() const {
    return normalizedNumber.isEmpty() ? number : normalizedNumber;
  }

  bool matchesNormalized(const String &candidate) const {
    return hasNormalized() && normalizedNumber.equalsIgnoreCase(candidate);
  }
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
  bool removeQuickDialEntry(const String &code);
  String getQuickDialNumber(const String &code) const;
  bool hasQuickDialEntry(const String &code) const;

  // Blocked numbers
  const std::vector<BlockedNumberEntry> &getBlockedNumbers() const {
    return _blockedNumbers;
  }
  bool addBlockedNumber(const String &number, const String &name = "");
  bool removeBlockedNumber(const String &number);
  bool isIncomingCallBlocked(const String &number) const;

  // Priority callers
  const std::vector<PriorityCallerEntry> &getPriorityCallers() const {
    return _priorityCallers;
  }
  bool addPriorityCaller(const String &number);
  bool removePriorityCaller(const String &number);
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
