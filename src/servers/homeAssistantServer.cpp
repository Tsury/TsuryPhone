#ifdef HOME_ASSISTANT_INTEGRATION

#include "homeAssistantServer.h"
#include "../config/corePhoneConfig.h"
#include "../core/phoneValidation.h"
#include "../core/state.h"
#include "../generated/phoneBook.h"
#include "../utils/logger.h"
#include "../utils/string.h"
#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <vector>

namespace {
  const constexpr int kHttpServerPort = 80;
  const constexpr char *kContentTypeJson = "application/json";
  const constexpr char *kConfigFile = "/ha_config.json";
  const constexpr int kJsonBufferSize = 1024; // Reduced from 2048
}

// Compact storage structure - only HA-specific config
// Note: phoneBook, blockedNumbers, and DnD settings are now handled by CorePhoneConfig
struct HaConfig {
  std::vector<WebhookEntry> webhooks;
  String haServerUrl;                // For webhook calls
  String deviceName = kHaDeviceName; // Device name for mDNS
  bool phoneBookInitialized = false;
};

static HaConfig haConfig;

// Forward declarations
void loadConfiguration();
void saveConfiguration();
void seedCorePhoneConfigFromLocal();

// Global HaConfigProvider instance
HaConfigProvider haConfigProvider;

// HaConfigProvider implementation
bool HaConfigProvider::isDndConfigEnabled() const {
  return g_corePhoneConfig->isDndForceEnabled() || g_corePhoneConfig->isDndScheduleEnabled();
}

bool HaConfigProvider::isDndForceEnabled() const {
  return g_corePhoneConfig->isDndForceEnabled();
}

bool HaConfigProvider::isDndScheduleEnabled() const {
  return g_corePhoneConfig->isDndScheduleEnabled();
}

void HaConfigProvider::getDndHours(int &startHour,
                                   int &startMinute,
                                   int &endHour,
                                   int &endMinute) const {
  // If we have runtime overrides, use those
  if (hasDndHoursOverride()) {
    startHour = _overrideStartHour;
    startMinute = _overrideStartMinute;
    endHour = _overrideEndHour;
    endMinute = _overrideEndMinute;
  } else {
    // Use CorePhoneConfig values
    g_corePhoneConfig->getDndHours(startHour, startMinute, endHour, endMinute);
  }
}

void HaConfigProvider::setDndHoursOverride(int startHour,
                                           int startMinute,
                                           int endHour,
                                           int endMinute) {
  _overrideStartHour = startHour;
  _overrideStartMinute = startMinute;
  _overrideEndHour = endHour;
  _overrideEndMinute = endMinute;
}

bool HaConfigProvider::hasDndHoursOverride() const {
  return _overrideStartHour != -1;
}

void HaConfigProvider::clearDndHoursOverride() {
  _overrideStartHour = -1;
  _overrideStartMinute = -1;
  _overrideEndHour = -1;
  _overrideEndMinute = -1;
}

bool HaConfigProvider::isNumberBlocked(const char *number) const {
  return g_corePhoneConfig->isNumberBlocked(String(number));
}

// Private helper methods - now use core config for shared quick dial entries
bool HaConfigProvider::isHaPhoneBookEntry(const char *number) const {
  return g_corePhoneConfig->hasQuickDialEntry(String(number));
}

bool HaConfigProvider::isPartialOfHaPhoneBookEntry(const char *number) const {
  return g_corePhoneConfig->hasPartialQuickDialMatch(String(number));
}

const char *HaConfigProvider::getHaPhoneBookNumberForEntry(const char *entry) const {
  auto quickDialEntry = g_corePhoneConfig->getQuickDialEntry(String(entry));
  if (!quickDialEntry.name.isEmpty()) {
    static String cachedResult = quickDialEntry.number;
    return cachedResult.c_str();
  }
  return nullptr;
}

bool HaConfigProvider::isWebhookEntry(const char *number) const {
  String numStr(number);
  for (const auto &hook : haConfig.webhooks) {
    if (hook.number == numStr) {
      return true;
    }
  }
  return false;
}

bool HaConfigProvider::isPartialOfWebhookEntry(const char *number) const {
  String numStr(number);
  for (const auto &hook : haConfig.webhooks) {
    if (hook.number.startsWith(numStr)) {
      return true;
    }
  }
  return false;
}

const char *HaConfigProvider::getWebhookIdForNumber(const char *number) const {
  static String cachedResult;
  String numStr(number);
  for (const auto &hook : haConfig.webhooks) {
    if (hook.number == numStr) {
      cachedResult = hook.webhookId;
      return cachedResult.c_str();
    }
  }
  return nullptr;
}

void HaConfigProvider::executeWebhook(const char *webhookId) const {
  Logger::infoln(F("Executing webhook: %s"), webhookId);

  if (haConfig.haServerUrl.length() == 0) {
    Logger::errorln(F("No Home Assistant server URL configured"));
    return;
  }

  HTTPClient http;
  String url = haConfig.haServerUrl + "/api/webhook/" + String(webhookId);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST("{}");

  if (httpResponseCode > 0) {
    Logger::infoln(
        F("Webhook executed successfully: %s (response: %d)"), webhookId, httpResponseCode);
  } else {
    Logger::errorln(F("Webhook execution failed: %s (error: %d)"), webhookId, httpResponseCode);
  }

  http.end();
}

HomeAssistantServer::HomeAssistantServer()
    : _server(80),
      _ws("/ws"),
      _uptime(0),
      _isInitialized(false),
      _stateChanged(false),
      _phoneController(nullptr) {
  memset(_stats, 0, sizeof(_stats));
}

bool HomeAssistantServer::isEnabled() const {
  return true; // Always enabled when HOME_ASSISTANT_INTEGRATION is compiled in
}

void HomeAssistantServer::setPhoneController(IPhoneController *controller) {
  _phoneController = controller;
}

void HomeAssistantServer::init() {
  Logger::infoln(F("Initializing Home Assistant Server..."));

  if (!SPIFFS.begin(true)) {
    Logger::errorln(F("SPIFFS initialization failed"));
    return;
  }

  loadConfiguration();
  setupRoutes();
  setupWebSocket();
  _server.begin();

  if (MDNS.begin(haConfig.deviceName.c_str())) {
    MDNS.addService("http", "tcp", kHttpServerPort);
    Logger::infoln(F("mDNS service started: %s.local"), haConfig.deviceName.c_str());
  } else {
    Logger::errorln(F("mDNS initialization failed"));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Logger::infoln(F("Home Assistant Server ready at http://%s/ (http://%s.local/)"),
                   WiFi.localIP().toString().c_str(),
                   haConfig.deviceName.c_str());
  } else {
    Logger::errorln(F("WiFi not connected during HA server initialization"));
  }

  _uptime = millis();
  _isInitialized = true;
}

void HomeAssistantServer::setupRoutes() {
  // Root endpoint - minimal response
  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String response = "{\"device\":\"" + String(kHaDeviceName) + "\",\"version\":\"1.0\"}";
    sendResponse(request, response.c_str());
  });

  // All GET endpoints use single handler
  _server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRequest(request, "status");
  });

  _server.on("/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRequest(request, "stats");
  });

  _server.on(
      "/dnd", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRequest(request, "dnd"); });

  _server.on("/phonebook", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRequest(request, "phonebook");
  });

  _server.on("/blocked", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRequest(request, "blocked");
  });

  _server.on("/webhooks", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRequest(request, "webhooks");
  });

  // All POST endpoints use single handler
  _server.on(
      "/action",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "action");
      });

  _server.on(
      "/webhooks",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "webhooks");
      });
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "webhooks");
      });

  // CORS
  _server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, kContentTypeJson, "{\"error\":\"Not found\"}");
    }
  });
}

void HomeAssistantServer::handleRequest(AsyncWebServerRequest *request, const char *endpoint) {
  JsonDocument doc;

  if (strcmp(endpoint, "status") == 0) {
    doc["state"] = appStateToString(_lastState.newAppState);
    doc["dnd"] = _lastState.isDnd;
    doc["maintenance"] = _lastState.isMaintenanceMode;
    doc["device_name"] = haConfig.deviceName;
    doc["uptime"] = millis() - _uptime;
    doc["free_heap"] = ESP.getFreeHeap();

    // WiFi information
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = WiFi.status() == WL_CONNECTED;
    if (WiFi.status() == WL_CONNECTED) {
      wifi["ssid"] = WiFi.SSID();
      wifi["rssi"] = WiFi.RSSI();
      wifi["ip"] = WiFi.localIP().toString();
      wifi["mac"] = WiFi.macAddress();
    }

    // Call information - enhanced to show call state better
    if (_lastState.callState.callNumber[0] != '\0') {
      JsonObject call = doc["call"].to<JsonObject>();
      call["number"] = _lastState.callState.callNumber;
      call["active"] = (_lastState.newAppState == AppState::InCall);
      call["id"] = _lastState.callState.callId;
      // Add call waiting info if available
      if (_lastState.callState.callWaitingId > 0) {
        call["has_waiting"] = true;
        call["waiting_id"] = _lastState.callState.callWaitingId;
      } else {
        call["has_waiting"] = false;
      }
    }
  } else if (strcmp(endpoint, "stats") == 0) {
    doc["uptime"] = millis() - _uptime;
    doc["calls"] = _stats[0];
    doc["incoming"] = _stats[1];
    doc["outgoing"] = _stats[2];
    doc["blocked"] = _stats[3];
    doc["resets"] = _stats[4];
    doc["free_heap"] = ESP.getFreeHeap();

    // Additional ESP32 system information
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["sketch_size"] = ESP.getSketchSize();
    doc["sketch_free"] = ESP.getFreeSketchSpace();
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    doc["sdk_version"] = ESP.getSdkVersion();
  } else if (strcmp(endpoint, "dnd") == 0) {
    if (g_corePhoneConfig) {
      const DndSettings &dndSettings = g_corePhoneConfig->getDndSettings();
      doc["force_enabled"] = dndSettings.force_enabled;
      doc["schedule_enabled"] = dndSettings.schedule_enabled;
      doc["start_hour"] = dndSettings.startHour;
      doc["start_minute"] = dndSettings.startMinute;
      doc["end_hour"] = dndSettings.endHour;
      doc["end_minute"] = dndSettings.endMinute;
    } else {
      // Fallback to default DnD hours from CorePhoneConfig
      doc["force_enabled"] = false;
      doc["schedule_enabled"] = false;
      int startHour, startMinute, endHour, endMinute;
      g_corePhoneConfig->getDndHours(startHour, startMinute, endHour, endMinute);
      doc["start_hour"] = startHour;
      doc["start_minute"] = startMinute;
      doc["end_hour"] = endHour;
      doc["end_minute"] = endMinute;
    }
    doc["currently_active"] = _lastState.isDnd;
  } else if (strcmp(endpoint, "phonebook") == 0) {
    JsonArray entries = doc["entries"].to<JsonArray>();
    // Use core phone config for shared quick dial entries
    auto quickDialEntries = g_corePhoneConfig->getAllQuickDialEntries();
    for (const auto &entry : quickDialEntries) {
      JsonObject entryObj = entries.add<JsonObject>();
      entryObj["name"] = entry.name;
      entryObj["number"] = entry.number;
    }
  } else if (strcmp(endpoint, "blocked") == 0) {
    JsonArray numbers = doc["blocked_numbers"].to<JsonArray>();
    // Use core phone config for shared blocked numbers
    if (g_corePhoneConfig) {
      auto blockedNumbers = g_corePhoneConfig->getBlockedNumbers();
      for (const String &number : blockedNumbers) {
        numbers.add(number);
      }
    }
  } else if (strcmp(endpoint, "webhooks") == 0) {
    JsonArray hooks = doc["webhooks"].to<JsonArray>();
    for (const auto &hook : haConfig.webhooks) {
      JsonObject hookObj = hooks.add<JsonObject>();
      hookObj["number"] = hook.number;
      hookObj["webhook_id"] = hook.webhookId;
    }
    doc["server_url"] = haConfig.haServerUrl;
  }

  String response;
  serializeJson(doc, response);
  sendResponse(request, response.c_str());
}

void HomeAssistantServer::handlePostRequest(AsyncWebServerRequest *request,
                                            uint8_t *data,
                                            size_t len,
                                            const char *endpoint) {
  if (!data || len == 0) {
    sendError(request, "Missing data");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
    sendError(request, "Invalid JSON");
    return;
  }

  if (strcmp(endpoint, "action") == 0) {
    String action = doc["action"] | "";

    // Reordered actions as requested:
    if (action == "call_custom") {
      String number = doc["number"] | "";
      if (number.length() > 0) {
        Logger::infoln(F("Custom call initiated: %s"), number.c_str());
        if (_phoneController) {
          _phoneController->performCall(number.c_str());
        }
        _stats[2]++; // outgoing calls
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "quick_call") {
      String entry = doc["entry"] | "";
      const char *number = haConfigProvider.getHaPhoneBookNumberForEntry(entry.c_str());
      if (number) {
        Logger::infoln(F("Quick call initiated: %s -> %s"), entry.c_str(), number);
        if (_phoneController) {
          _phoneController->performCall(number);
        }
        _stats[2]++; // outgoing calls
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Entry not found");
      }
    } else if (action == "add_quick_dial") {
      String name = doc["name"] | "";
      String number = doc["number"] | "";
      if (name.length() > 0 && number.length() > 0) {
        // Check for conflicts - use core config for shared quick dial entries
        if (haConfigProvider.isHaPhoneBookEntry(number.c_str()) ||
            haConfigProvider.isWebhookEntry(number.c_str())) {
          sendError(request, "Number already exists");
          return;
        }
        // Add to core phone config for shared access across all servers
        if (_phoneController) {
          Logger::infoln(F("Adding quick dial entry: %s -> %s"), name.c_str(), number.c_str());
          _phoneController->addQuickDialEntry(name.c_str(), number.c_str());
          broadcastStateUpdate(); // Immediate update for UI responsiveness
        } else {
          sendError(request, "Phone controller not available");
          return;
        }
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing name or number");
      }
    } else if (action == "remove_quick_dial") {
      String name = doc["name"] | "";
      if (_phoneController) {
        Logger::infoln(F("Removing quick dial entry: %s"), name.c_str());
        _phoneController->removeQuickDialEntry(name.c_str());
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Phone controller not available");
      }
    } else if (action == "add_blocked") {
      String number = doc["number"] | "";
      if (number.length() > 0) {
        Logger::infoln(F("Adding blocked number: %s"), number.c_str());
        g_corePhoneConfig->addBlockedNumber(number);
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "remove_blocked") {
      String number = doc["number"] | "";
      if (g_corePhoneConfig->removeBlockedNumber(number)) {
        Logger::infoln(F("Removed blocked number: %s"), number.c_str());
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Number not found");
      }
    } else if (action == "webhook_add") {
      String number = doc["number"] | "";
      String webhookId = doc["webhook_id"] | "";

      if (number.length() > 0 && webhookId.length() > 0) {
        if (haConfigProvider.isHaPhoneBookEntry(number.c_str()) ||
            haConfigProvider.isWebhookEntry(number.c_str())) {
          sendError(request, "Number already exists");
          return;
        }
        Logger::infoln(F("Adding webhook: %s -> %s"), number.c_str(), webhookId.c_str());
        addWebhookEntry(number.c_str(), webhookId.c_str());
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number or webhook_id");
      }
    } else if (action == "webhook_remove") {
      String number = doc["number"] | "";
      if (number.length() > 0) {
        Logger::infoln(F("Removing webhook for number: %s"), number.c_str());
        removeWebhookEntry(number.c_str());
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "hangup") {
      Logger::infoln(F("Hangup action initiated"));
      if (_phoneController) {
        _phoneController->performHangup();
      }
      sendResponse(request, "{\"success\":true}");
    } else if (action == "switch_call_waiting") {
      Logger::infoln(F("Call waiting switch initiated"));
      if (_phoneController) {
        _phoneController->performSwitchToCallWaiting();
      }
      sendResponse(request, "{\"success\":true}");
    } else if (action == "dnd_force") {
      bool enabled = doc["enabled"] | false;
      Logger::infoln(F("DnD Force mode set to: %s"), enabled ? F("enabled") : F("disabled"));
      g_corePhoneConfig->setDndForceEnabled(enabled);
      broadcastStateUpdate(); // Immediate update for UI responsiveness
      sendResponse(request, "{\"success\":true}");
    } else if (action == "dnd_schedule") {
      bool enabled = doc["enabled"] | false;
      Logger::infoln(F("DnD Schedule mode set to: %s"), enabled ? F("enabled") : F("disabled"));
      g_corePhoneConfig->setDndScheduleEnabled(enabled);
      broadcastStateUpdate(); // Immediate update for UI responsiveness
      sendResponse(request, "{\"success\":true}");
    } else if (action == "dnd_start_time") {
      int hour = doc["hour"] | -1;
      int minute = doc["minute"] | -1;
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
        Logger::infoln(F("DnD Start time set to: %02d:%02d"), hour, minute);
        int startHour, startMinute, endHour, endMinute;
        g_corePhoneConfig->getDndHours(startHour, startMinute, endHour, endMinute);
        g_corePhoneConfig->setDndHours(hour, minute, endHour, endMinute);
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Invalid time");
      }
    } else if (action == "dnd_end_time") {
      int hour = doc["hour"] | -1;
      int minute = doc["minute"] | -1;
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
        Logger::infoln(F("DnD End time set to: %02d:%02d"), hour, minute);
        int startHour, startMinute, endHour, endMinute;
        g_corePhoneConfig->getDndHours(startHour, startMinute, endHour, endMinute);
        g_corePhoneConfig->setDndHours(startHour, startMinute, hour, minute);
        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Invalid time");
      }
    } else if (action == "dnd_hours") {
      int startHour = doc["start_hour"] | -1;
      int startMinute = doc["start_minute"] | -1;
      int endHour = doc["end_hour"] | -1;
      int endMinute = doc["end_minute"] | -1;
      if (startHour >= 0 && startHour < 24 && startMinute >= 0 && startMinute < 60 &&
          endHour >= 0 && endHour < 24 && endMinute >= 0 && endMinute < 60) {
        if (g_corePhoneConfig) {
          Logger::infoln(F("DnD Hours set to: %02d:%02d - %02d:%02d"), startHour, startMinute, endHour, endMinute);
          g_corePhoneConfig->setDndHours(startHour, startMinute, endHour, endMinute);
          broadcastStateUpdate(); // Immediate update for UI responsiveness
          sendResponse(request, "{\"success\":true}");
        } else {
          sendError(request, "Core config not available");
        }
      } else {
        sendError(request, "Invalid time");
      }
    } else if (action == "ring_pattern") {
      // Handle new structured pattern data from HA integration
      JsonArray durationsArray = doc["durations"];
      int repeats = doc["repeats"] | 1;

      if (durationsArray.size() == 0) {
        sendError(request, "Invalid pattern data - no durations");
        return;
      }

      // Convert durations array to vector
      std::vector<int> durations;
      for (JsonVariant duration : durationsArray) {
        int dur = duration.as<int>();
        if (dur <= 0 || dur > 30000) {
          sendError(request, "Invalid duration value");
          return;
        }
        durations.push_back(dur);
      }

      if (repeats <= 0 || repeats > 100) {
        sendError(request, "Invalid repeat count");
        return;
      }

      // Validate pattern logic for repeats
      if (repeats > 1 && durations.size() % 2 != 0) {
        sendError(request, "Pattern with repeats must have even number of durations");
        return;
      }

      // Create RingPattern directly instead of parsing string
      RingPattern pattern;
      pattern.durations = durations;
      pattern.repeats = repeats;
      pattern.isValid = true;

      Logger::infoln(F("Ring pattern triggered: %d durations, %d repeats"), durations.size(), repeats);
      if (_phoneController) {
        _phoneController->performRingWithStructuredPattern(pattern);
      }
      sendResponse(request, "{\"success\":true}");
    } else if (action == "refresh_data") {
      Logger::infoln(F("Data refresh requested"));
      broadcastStateUpdate();
      sendResponse(request, "{\"success\":true}");
    } else if (action == "maintenance_mode") {
      bool enabled = doc["enabled"] | false;
      Logger::infoln(F("Maintenance mode set to: %s"), enabled ? F("enabled") : F("disabled"));
      if (_phoneController) {
        _phoneController->performSetMaintenanceMode(enabled);
        broadcastStateUpdate(); // Immediate update for UI responsiveness
      }
      sendResponse(request, "{\"success\":true}");
    } else if (action == "set_device_name") {
      String deviceName = doc["device_name"] | "";
      if (deviceName.length() > 0) {
        Logger::infoln(F("Device name set to: %s"), deviceName.c_str());
        haConfig.deviceName = deviceName;
        saveConfiguration();

        // Restart mDNS with new name
        MDNS.end();
        if (MDNS.begin(haConfig.deviceName.c_str())) {
          MDNS.addService("http", "tcp", kHttpServerPort);
          Logger::infoln(F("Device name updated: %s.local"), haConfig.deviceName.c_str());
        }

        broadcastStateUpdate(); // Immediate update for UI responsiveness
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing device_name");
      }
    } else if (action == "reset") {
      Logger::infoln(F("Device reset initiated"));
      if (_phoneController) {
        _phoneController->performReset();
      }
      sendResponse(request, "{\"success\":true}");
    } else {
      sendError(request, "Unknown action");
    }
  } else if (strcmp(endpoint, "webhooks") == 0) {
    String serverUrl = doc["server_url"] | "";
    if (serverUrl.length() > 0) {
      Logger::infoln(F("Webhook server URL updated: %s"), serverUrl.c_str());
      haConfig.haServerUrl = serverUrl;
      saveConfiguration();
      sendResponse(request, "{\"success\":true}");
    } else {
      sendError(request, "Missing server_url");
    }
  }

  broadcastStateUpdate();
}

RingPattern HomeAssistantServer::parseRingPattern(const String &pattern) {
  RingPattern rp;

  // Check for repeat syntax: pattern/num
  int slashPos = pattern.indexOf('/');
  String mainPattern = pattern;
  int repeats = 1;

  if (slashPos > 0) {
    mainPattern = pattern.substring(0, slashPos);
    repeats = pattern.substring(slashPos + 1).toInt();
    if (repeats <= 0) {
      repeats = 1;
    }
  }

  // Parse comma-separated durations
  int startPos = 0;
  int commaPos;
  std::vector<int> durations;

  do {
    commaPos = mainPattern.indexOf(',', startPos);
    String durStr = (commaPos > 0) ? mainPattern.substring(startPos, commaPos)
                                   : mainPattern.substring(startPos);
    int duration = durStr.toInt();

    if (duration <= 0 || duration > 30000) { // Max 30 seconds per duration
      return rp;                             // Invalid
    }

    durations.push_back(duration);
    startPos = commaPos + 1;
  } while (commaPos > 0);

  // Validate pattern
  if (durations.empty()) {
    return rp; // Invalid
  }

  // If there are repeats, pattern must end with even number (pause)
  if (repeats > 1 && durations.size() % 2 != 0) {
    return rp; // Invalid
  }

  rp.durations = durations;
  rp.repeats = repeats;
  rp.isValid = true;

  return rp;
}

void HomeAssistantServer::sendResponse(AsyncWebServerRequest *request, const char *data, int code) {
  request->send(code, kContentTypeJson, data);
}

void HomeAssistantServer::sendError(AsyncWebServerRequest *request, const char *error, int code) {
  String response = "{\"error\":\"";
  response += error;
  response += "\"}";
  sendResponse(request, response.c_str(), code);
}

void HomeAssistantServer::setupWebSocket() {
  _ws.onEvent([this](AsyncWebSocket *server,
                     AsyncWebSocketClient *client,
                     AwsEventType type,
                     void *arg,
                     uint8_t *data,
                     size_t len) {
    if (type == WS_EVT_CONNECT) {
      Logger::infoln(F("WebSocket client connected: %s"), client->remoteIP().toString().c_str());
      // Send initial state to newly connected client
      broadcastStateUpdate();
    } else if (type == WS_EVT_DISCONNECT) {
      Logger::infoln(F("WebSocket client disconnected"));
    } else if (type == WS_EVT_ERROR) {
      Logger::errorln(F("WebSocket error on client %u"), client->id());
    }
  });
  _server.addHandler(&_ws);
}

void HomeAssistantServer::process(const State &state) {
  if (!_isInitialized) {
    return;
  }

  // Check for state changes
  bool stateChanged = false;

  if (_lastState.newAppState != state.newAppState || _lastState.isDnd != state.isDnd ||
      _lastState.isMaintenanceMode != state.isMaintenanceMode ||
      strcmp(_lastState.callState.callNumber, state.callState.callNumber) != 0) {
    stateChanged = true;
  }

  // Track statistics
  AppState prevState = _lastState.newAppState;
  if (prevState != AppState::InCall && state.newAppState == AppState::InCall) {
    _stats[0]++; // total calls
  }
  if (prevState != AppState::IncomingCall && state.newAppState == AppState::IncomingCall) {
    _stats[1]++; // incoming calls
  }

  _lastState = state;

  if (stateChanged) {
    Logger::infoln(F("State change detected - broadcasting update"));
    broadcastStateUpdate();
  }

  // Periodic WebSocket client cleanup to remove dead connections
  static uint32_t lastCleanup = 0;

  if (millis() - lastCleanup > 30000) {
    uint32_t beforeCount = _ws.count();
    _ws.cleanupClients();
    uint32_t afterCount = _ws.count();
    lastCleanup = millis();

    if (beforeCount != afterCount) {
      Logger::infoln(F("WebSocket cleanup: removed %d dead clients"), beforeCount - afterCount);
    }
  }
}

void HomeAssistantServer::notifyBlockedCall(const char *number) {
  Logger::infoln(F("Blocked call from: %s"), number);
  _stats[3]++; // blocked calls
  broadcastStateUpdate();
}

// Webhook methods implementation
bool HomeAssistantServer::hasWebhookEntry(const char *number) const {
  return haConfigProvider.isWebhookEntry(number);
}

bool HomeAssistantServer::hasPartialWebhookEntry(const char *number) const {
  return haConfigProvider.isPartialOfWebhookEntry(number);
}

bool HomeAssistantServer::executeWebhook(const char *number) const {
  if (haConfigProvider.isWebhookEntry(number)) {
    const char *webhookId = haConfigProvider.getWebhookIdForNumber(number);
    if (webhookId) {
      haConfigProvider.executeWebhook(webhookId);
      return true;
    }
  }
  return false;
}

void HomeAssistantServer::broadcastStateUpdate() {
  if (_ws.count() > 0) {
    // Send comprehensive state update via WebSocket
    JsonDocument doc;
    doc["state"] = appStateToString(_lastState.newAppState);
    doc["dnd"] = _lastState.isDnd;
    doc["maintenance"] = _lastState.isMaintenanceMode;
    doc["uptime"] = millis() - _uptime;
    doc["free_heap"] = ESP.getFreeHeap();

    // Call information
    if (_lastState.callState.callNumber[0] != '\0') {
      JsonObject call = doc["call"].to<JsonObject>();
      call["number"] = _lastState.callState.callNumber;
      call["active"] = (_lastState.newAppState == AppState::InCall);
      call["id"] = _lastState.callState.callId;

      if (_lastState.callState.callWaitingId > 0) {
        call["has_waiting"] = true;
        call["waiting_id"] = _lastState.callState.callWaitingId;
      } else {
        call["has_waiting"] = false;
      }
    }

    String response;
    serializeJson(doc, response);

    _ws.textAll(response);
  }
}

// Configuration management - only HA-specific settings
void loadConfiguration() {
  if (!SPIFFS.exists(kConfigFile)) {
    // Seed core phone config from local phonebook if not already initialized
    if (g_corePhoneConfig && !haConfig.phoneBookInitialized) {
      seedCorePhoneConfigFromLocal();
      haConfig.phoneBookInitialized = true;
    }
    saveConfiguration();
    return;
  }

  File file = SPIFFS.open(kConfigFile, "r");
  if (!file) {
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    file.close();
    return;
  }

  // Load only HA-specific configuration (DnD settings now in CorePhoneConfig)
  haConfig.phoneBookInitialized = doc["phonebook_initialized"] | false;
  haConfig.haServerUrl = doc["ha_server_url"] | "";
  haConfig.deviceName = doc["device_name"] | kHaDeviceName;

  // Note: phonebook and blocked numbers are now handled by CorePhoneConfig
  // Only load HA-specific data (DnD, webhooks, device settings)

  // Load webhooks (HA-specific functionality)
  if (doc["webhooks"].is<JsonArray>()) {
    haConfig.webhooks.clear();
    for (JsonObject hook : doc["webhooks"].as<JsonArray>()) {
      WebhookEntry entry;
      entry.number = hook["number"].as<String>();
      entry.webhookId = hook["webhook_id"].as<String>();
      haConfig.webhooks.push_back(entry);
    }
  }

  // Seed core phone config if needed (only once)
  if (g_corePhoneConfig && !haConfig.phoneBookInitialized) {
    seedCorePhoneConfigFromLocal();
    haConfig.phoneBookInitialized = true;
    saveConfiguration();
  }

  file.close();
}

void saveConfiguration() {
  JsonDocument doc;

  // Save only HA-specific configuration (DnD settings now in CorePhoneConfig)
  doc["phonebook_initialized"] = haConfig.phoneBookInitialized;
  doc["ha_server_url"] = haConfig.haServerUrl;
  doc["device_name"] = haConfig.deviceName;

  // Note: phonebook and blocked numbers are now saved by CorePhoneConfig
  // Only save HA-specific data (webhooks)
  JsonArray webhooks = doc["webhooks"].to<JsonArray>();
  for (const auto &hook : haConfig.webhooks) {
    JsonObject hookObj = webhooks.add<JsonObject>();
    hookObj["number"] = hook.number;
    hookObj["webhook_id"] = hook.webhookId;
  }

  File file = SPIFFS.open(kConfigFile, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

void seedCorePhoneConfigFromLocal() {
  // Seed core phone config with generated phonebook entries
  if (!g_corePhoneConfig) {
    return;
  }

  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; i++) {
    QuickDialEntry entry;
    entry.name = String(phoneBookEntries[i].entry);
    entry.number = String(phoneBookEntries[i].number);
    g_corePhoneConfig->addQuickDialEntry(entry);
  }
}

// Internal webhook management functions - these are kept for internal HA server use
void addWebhookEntry(const char *number, const char *webhookId) {
  WebhookEntry entry;
  entry.number = String(number);
  entry.webhookId = String(webhookId);
  haConfig.webhooks.push_back(entry);
  saveConfiguration();
}

void removeWebhookEntry(const char *number) {
  String numStr(number);
  auto it = std::find_if(haConfig.webhooks.begin(),
                         haConfig.webhooks.end(),
                         [&](const WebhookEntry &hook) { return hook.number == numStr; });
  if (it != haConfig.webhooks.end()) {
    haConfig.webhooks.erase(it);
    saveConfiguration();
  }
}

#endif // HOME_ASSISTANT_INTEGRATION
