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
  BlockedNumbers,
  WebhookActions,
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
  const std::map<String, String> &getQuickDialEntries() const {
    return _quickDialEntries;
  }
  bool addQuickDialEntry(const String &code, const String &number);
  bool removeQuickDialEntry(const String &code);
  String getQuickDialNumber(const String &code) const;
  bool hasQuickDialEntry(const String &code) const;

  // Blocked numbers
  const std::vector<String> &getBlockedNumbers() const {
    return _blockedNumbers;
  }
  bool addBlockedNumber(const String &number);
  bool removeBlockedNumber(const String &number);
  bool isIncomingCallBlocked(const String &number) const;

  // Webhook actions
  const std::map<String, String> &getWebhookActions() const {
    return _webhookActions;
  }
  bool addWebhookAction(const String &code, const String &webhookId);
  bool removeWebhookAction(const String &code);
  String getWebhookId(const String &code) const;
  bool hasWebhookAction(const String &code) const;

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

  // Statistics
  int getResetCount() const {
    return _resetCount;
  }
  void incrementResetCount();

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
  std::map<String, String> _quickDialEntries;
  std::vector<String> _blockedNumbers;
  std::map<String, String> _webhookActions;
  String _ringPattern;
  String _homeAssistantUrl;
  int _resetCount;

  std::function<void(ConfigChangeType)> _configChangeCallback;

  static const char *kConfigFilePath;
  static const char *kDefaultRingPattern;
};
