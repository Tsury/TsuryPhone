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
  DeviceName
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
  String name;

  QuickDialEntry() = default;
  QuickDialEntry(const String &c, const String &n, const String &nm = "")
      : code(c), number(n), name(nm) {}
};

struct BlockedNumberEntry {
  String number;
  String reason;

  BlockedNumberEntry() = default;
  BlockedNumberEntry(const String &n, const String &r = "") : number(n), reason(r) {}
};

class DeviceConfig {
public:
  DeviceConfig();

  bool init();
  bool save();
  bool load();

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
  bool addBlockedNumber(const String &number, const String &reason = "");
  bool removeBlockedNumber(const String &number);
  bool isIncomingCallBlocked(const String &number) const;

  // Priority callers
  const std::vector<String> &getPriorityCallers() const {
    return _priorityCallers;
  }
  bool addPriorityCaller(const String &number);
  bool removePriorityCaller(const String &number);
  bool isPriorityCaller(const String &number) const;

  struct NumberClassification {
    bool isBlocked = false;
    bool isPriority = false;
  };
  NumberClassification classifyNumber(const String &number) const;

  // Ring pattern
  String getRingPattern() const {
    return _ringPattern;
  }
  void setRingPattern(const String &pattern);

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

  String _deviceId;
  AudioConfig _audioConfig;
  DndConfig _dndConfig;
  std::vector<QuickDialEntry> _quickDialEntries;
  std::vector<BlockedNumberEntry> _blockedNumbers;
  std::vector<String> _priorityCallers; // numbers that bypass DND / special handling
  String _ringPattern;
  String _homeAssistantUrl;

  std::function<void(ConfigChangeType)> _configChangeCallback;

  static const char *kConfigFilePath;
  static const char *kDefaultRingPattern;
};
