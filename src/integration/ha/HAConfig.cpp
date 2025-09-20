#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAConfig.h"
#include "../../common/logger.h"
#include "../../core/DeviceConfig.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <algorithm>

const char *HAConfig::kConfigFilePath = "/ha_config.json";

HAConfig::HAConfig(DeviceConfig &baseConfig) : _baseConfig(baseConfig) {}

bool HAConfig::init() {
  Logger::infoln(F("Initializing HA configuration"));

  if (!load()) {
    Logger::infoln(F("No existing HA config found, initializing with defaults"));
    initializeDefaults();
  } else {
    Logger::infoln(F("HA Configuration loaded successfully"));
  }

  return true;
}

bool HAConfig::save() {
  JsonDocument doc;

  // Webhook actions
  JsonArray webhooks = doc["webhookActions"].to<JsonArray>();
  for (const auto &entry : _webhookActions) {
    JsonObject entryObj = webhooks.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["id"] = entry.id;
    entryObj["actionName"] = entry.actionName;
  }

  // Home Assistant URL
  doc["homeAssistantUrl"] = _homeAssistantUrl;

  File file = SPIFFS.open(kConfigFilePath, "w");
  if (!file) {
    Logger::errorln(F("Failed to open HA config file for writing"));
    return false;
  }

  if (serializeJson(doc, file) == 0) {
    Logger::errorln(F("Failed to write HA config to file"));
    file.close();
    return false;
  }

  file.close();
  Logger::infoln(F("HA Configuration saved successfully"));
  return true;
}

bool HAConfig::load() {
  if (!SPIFFS.exists(kConfigFilePath)) {
    Logger::infoln(F("HA Config file does not exist: %s"), kConfigFilePath);
    return false;
  }

  File file = SPIFFS.open(kConfigFilePath, "r");
  if (!file) {
    Logger::errorln(F("Failed to open HA config file for reading"));
    return false;
  }

  String fileContent = file.readString();
  file.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, fileContent);

  if (error) {
    Logger::errorln(F("Failed to parse HA config file: %s"), error.c_str());
    return false;
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
    Logger::infoln(F("Loaded %d HA webhook actions"), _webhookActions.size());
  }

  // Home Assistant URL
  if (doc["homeAssistantUrl"].is<const char *>()) {
    _homeAssistantUrl = doc["homeAssistantUrl"].as<String>();
  }

  return true;
}

void HAConfig::initializeDefaults() {
  _homeAssistantUrl = "http://homeassistant.local:8123";
}

bool HAConfig::addWebhookAction(const String &code,
                                const String &webhookId,
                                const String &actionName) {
  if (isCodeConflict(code)) {
    return false;
  }

  WebhookActionEntry entry(code, webhookId, actionName);
  _webhookActions.push_back(entry);
  saveAndNotify();
  return true;
}

bool HAConfig::removeWebhookAction(const String &code) {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  if (it != _webhookActions.end()) {
    _webhookActions.erase(it);
    saveAndNotify();
    return true;
  }
  return false;
}

String HAConfig::getWebhookId(const String &code) const {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  return (it != _webhookActions.end()) ? it->id : String();
}

bool HAConfig::hasWebhookAction(const String &code) const {
  auto it = std::find_if(_webhookActions.begin(),
                         _webhookActions.end(),
                         [&code](const WebhookActionEntry &entry) { return entry.code == code; });
  return it != _webhookActions.end();
}

bool HAConfig::isCodeConflict(const String &code) const {
  // Check for conflicts with base config (quick dial)
  if (_baseConfig.hasQuickDialEntry(code)) {
    return true;
  }

  // Check for conflicts with existing webhook actions
  return hasWebhookAction(code);
}

void HAConfig::setHomeAssistantUrl(const String &url) {
  if (_homeAssistantUrl != url) {
    _homeAssistantUrl = url;
    save(); // Save immediately
  }
}

void HAConfig::saveAndNotify() {
  save();
  if (_configChangeCallback) {
    _configChangeCallback();
  }
}

#endif // HOME_ASSISTANT_INTEGRATION
