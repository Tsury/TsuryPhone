#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <Arduino.h>
#include <functional>
#include <vector>

// Forward declaration
class DeviceConfig;

struct WebhookActionEntry {
  String code;
  String id;
  String actionName;

  WebhookActionEntry() = default;
  WebhookActionEntry(const String &c, const String &i, const String &an = "")
      : code(c), id(i), actionName(an) {}
};

/**
 * Home Assistant specific configuration manager
 * Handles webhook actions and other HA-specific settings
 */
class HAConfig {
public:
  HAConfig(DeviceConfig &baseConfig);

  // Lifecycle
  bool init();
  bool save();
  bool load();

  // Webhook actions
  const std::vector<WebhookActionEntry> &getWebhookActions() const {
    return _webhookActions;
  }
  bool addWebhookAction(const String &code, const String &webhookId, const String &actionName = "");
  bool removeWebhookAction(const String &code);
  String getWebhookId(const String &code) const;
  bool hasWebhookAction(const String &code) const;

  // Code conflict checking (delegates to base config)
  bool isCodeConflict(const String &code) const;

  // Home Assistant URL
  String getHomeAssistantUrl() const {
    return _homeAssistantUrl;
  }
  void setHomeAssistantUrl(const String &url);

  // Configuration change callbacks
  void setConfigChangeCallback(std::function<void()> callback) {
    _configChangeCallback = callback;
  }

private:
  void initializeDefaults();
  void saveAndNotify();

  DeviceConfig &_baseConfig;
  std::vector<WebhookActionEntry> _webhookActions;
  String _homeAssistantUrl;

  std::function<void()> _configChangeCallback;

  static const char *kConfigFilePath;
};

#endif // HOME_ASSISTANT_INTEGRATION
