#ifdef HOME_ASSISTANT_INTEGRATION

#include "homeAssistantServer.h"
#include "../generated/phoneBook.h"
#include "logger.h"
#include "phoneBook.h"
#include "state.h"
#include "string.h"
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

// Compact storage structure
struct HaConfig {
  bool dndForceEnabled = false;
  bool dndScheduleEnabled = false;
  uint8_t dndStartHour = kDndStartHour;
  uint8_t dndStartMinute = kDndStartMinute;
  uint8_t dndEndHour = kDndEndHour;
  uint8_t dndEndMinute = kDndEndMinute;
  std::vector<std::pair<String, String>> phoneBook;
  std::vector<String> blockedNumbers;
  std::vector<WebhookEntry> webhooks;
  String haServerUrl;                // For webhook calls
  String deviceName = kHaDeviceName; // Device name for mDNS
  bool phoneBookInitialized = false;
};

static HaConfig haConfig;

// Forward declarations
void loadConfiguration();
void saveConfiguration();
void seedHaPhoneBookFromLocal();

HomeAssistantServer haServer;

// External functions - declared but implemented elsewhere
extern void haPerformCall(const char *number);
extern void haPerformHangup();
extern void haPerformReset();
extern void haPerformRingWithStructuredPattern(const RingPattern &pattern);
extern void haPerformSetMaintenanceMode(bool enabled);
extern void haPerformSwitchToCallWaiting();
extern void haSetDndHours(int startHour, int startMinute, int endHour, int endMinute);

HomeAssistantServer::HomeAssistantServer()
    : _server(80), _ws("/ws"), _uptime(0), _isInitialized(false), _stateChanged(false) {
  memset(_stats, 0, sizeof(_stats));
}

void HomeAssistantServer::init() {
  Logger::infoln(F("Initializing HA Server..."));

  if (!SPIFFS.begin(true)) {
    Logger::errorln(F("SPIFFS failed"));
    return;
  }

  loadConfiguration();
  setupRoutes();
  setupWebSocket();
  _server.begin();

  if (MDNS.begin(haConfig.deviceName.c_str())) {
    MDNS.addService("http", "tcp", kHttpServerPort);
    Logger::infoln(F("mDNS: %s.local"), haConfig.deviceName.c_str());
  }

  _uptime = millis();
  _isInitialized = true;
  Logger::infoln(F("HA Server ready"));
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
      "/dnd",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "dnd");
      });

  _server.on(
      "/phonebook",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "phonebook");
      });

  _server.on(
      "/blocked",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePostRequest(request, data, len, "blocked");
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
    if (_lastState.callState.callNumber[0] != '\0') {
      doc["call"]["number"] = _lastState.callState.callNumber;
      doc["call"]["active"] = (_lastState.newAppState == AppState::InCall);
    }
  } else if (strcmp(endpoint, "stats") == 0) {
    doc["uptime"] = millis() - _uptime;
    doc["calls"] = _stats[0];
    doc["incoming"] = _stats[1];
    doc["outgoing"] = _stats[2];
    doc["blocked"] = _stats[3];
    doc["resets"] = _stats[4];
    doc["free_heap"] = ESP.getFreeHeap();
  } else if (strcmp(endpoint, "dnd") == 0) {
    doc["force_enabled"] = haConfig.dndForceEnabled;
    doc["schedule_enabled"] = haConfig.dndScheduleEnabled;
    doc["start_hour"] = haConfig.dndStartHour;
    doc["start_minute"] = haConfig.dndStartMinute;
    doc["end_hour"] = haConfig.dndEndHour;
    doc["end_minute"] = haConfig.dndEndMinute;
    doc["currently_active"] = _lastState.isDnd;
  } else if (strcmp(endpoint, "phonebook") == 0) {
    JsonArray entries = doc["entries"].to<JsonArray>();
    for (const auto &entry : haConfig.phoneBook) {
      JsonObject entryObj = entries.add<JsonObject>();
      entryObj["name"] = entry.first;
      entryObj["number"] = entry.second;
    }
  } else if (strcmp(endpoint, "blocked") == 0) {
    JsonArray numbers = doc["blocked_numbers"].to<JsonArray>();
    for (const String &number : haConfig.blockedNumbers) {
      numbers.add(number);
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
        haPerformCall(number.c_str());
        _stats[2]++; // outgoing calls
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "quick_call") {
      String entry = doc["entry"] | "";
      const char *number = getHaPhoneBookNumberForEntry(entry.c_str());
      if (number) {
        haPerformCall(number);
        _stats[2]++; // outgoing calls
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Entry not found");
      }
    } else if (action == "add_quick_dial") {
      String name = doc["name"] | "";
      String number = doc["number"] | "";
      if (name.length() > 0 && number.length() > 0) {
        // Check for conflicts
        if (isHaPhoneBookEntry(number.c_str()) || isWebhookEntry(number.c_str())) {
          sendError(request, "Number already exists");
          return;
        }
        haConfig.phoneBook.push_back({name, number});
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing name or number");
      }
    } else if (action == "remove_quick_dial") {
      String name = doc["name"] | "";
      auto it = std::find_if(haConfig.phoneBook.begin(),
                             haConfig.phoneBook.end(),
                             [&](const std::pair<String, String> &p) { return p.first == name; });
      if (it != haConfig.phoneBook.end()) {
        haConfig.phoneBook.erase(it);
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Entry not found");
      }
    } else if (action == "add_blocked") {
      String number = doc["number"] | "";
      if (number.length() > 0) {
        haConfig.blockedNumbers.push_back(number);
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "remove_blocked") {
      String number = doc["number"] | "";
      auto it = std::find(haConfig.blockedNumbers.begin(), haConfig.blockedNumbers.end(), number);
      if (it != haConfig.blockedNumbers.end()) {
        haConfig.blockedNumbers.erase(it);
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Number not found");
      }
    } else if (action == "webhook_add") {
      String number = doc["number"] | "";
      String webhookId = doc["webhook_id"] | "";
      if (number.length() > 0 && webhookId.length() > 0) {
        // Check for conflicts
        if (isHaPhoneBookEntry(number.c_str()) || isWebhookEntry(number.c_str())) {
          sendError(request, "Number already exists");
          return;
        }
        addWebhookEntry(number.c_str(), webhookId.c_str());
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number or webhook_id");
      }
    } else if (action == "webhook_remove") {
      String number = doc["number"] | "";
      if (number.length() > 0) {
        removeWebhookEntry(number.c_str());
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing number");
      }
    } else if (action == "hangup") {
      haPerformHangup();
      sendResponse(request, "{\"success\":true}");
    } else if (action == "switch_call_waiting") {
      haPerformSwitchToCallWaiting();
      sendResponse(request, "{\"success\":true}");
    } else if (action == "dnd_schedule") {
      bool enabled = doc["enabled"] | false;
      haConfig.dndScheduleEnabled = enabled;
      saveConfiguration();
      sendResponse(request, "{\"success\":true}");
    } else if (action == "dnd_start_time") {
      int hour = doc["hour"] | -1;
      int minute = doc["minute"] | -1;
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
        haConfig.dndStartHour = hour;
        haConfig.dndStartMinute = minute;
        haSetDndHours(haConfig.dndStartHour,
                      haConfig.dndStartMinute,
                      haConfig.dndEndHour,
                      haConfig.dndEndMinute);
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Invalid time");
      }
    } else if (action == "dnd_end_time") {
      int hour = doc["hour"] | -1;
      int minute = doc["minute"] | -1;
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
        haConfig.dndEndHour = hour;
        haConfig.dndEndMinute = minute;
        haSetDndHours(haConfig.dndStartHour,
                      haConfig.dndStartMinute,
                      haConfig.dndEndHour,
                      haConfig.dndEndMinute);
        saveConfiguration();
        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Invalid time");
      }
    } else if (action == "dnd_force") {
      bool enabled = doc["enabled"] | false;
      haConfig.dndForceEnabled = enabled;
      saveConfiguration();
      sendResponse(request, "{\"success\":true}");
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

      haPerformRingWithStructuredPattern(pattern);
      sendResponse(request, "{\"success\":true}");
    } else if (action == "refresh_data") {
      broadcastStateUpdate();
      sendResponse(request, "{\"success\":true}");
    } else if (action == "maintenance_mode") {
      bool enabled = doc["enabled"] | false;
      haPerformSetMaintenanceMode(enabled);
      sendResponse(request, "{\"success\":true}");
    } else if (action == "set_device_name") {
      String deviceName = doc["device_name"] | "";
      if (deviceName.length() > 0) {
        haConfig.deviceName = deviceName;
        saveConfiguration();

        // Restart mDNS with new name
        MDNS.end();
        if (MDNS.begin(haConfig.deviceName.c_str())) {
          MDNS.addService("http", "tcp", kHttpServerPort);
          Logger::infoln(F("mDNS updated: %s.local"), haConfig.deviceName.c_str());
        }

        sendResponse(request, "{\"success\":true}");
      } else {
        sendError(request, "Missing device_name");
      }
    } else if (action == "reset") {
      haPerformReset();
      sendResponse(request, "{\"success\":true}");
    } else {
      sendError(request, "Unknown action");
    }
  } else if (strcmp(endpoint, "webhooks") == 0) {
    String serverUrl = doc["server_url"] | "";
    if (serverUrl.length() > 0) {
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
      Logger::infoln(F("WS client connected: %u"), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      Logger::infoln(F("WS client disconnected: %u"), client->id());
    }
  });
  _server.addHandler(&_ws);
}

void HomeAssistantServer::process() {
  if (!_isInitialized) {
    return;
  }

  static uint32_t lastCleanup = 0;
  if (millis() - lastCleanup > 30000) {
    _ws.cleanupClients();
    lastCleanup = millis();
  }
}

void HomeAssistantServer::updateState(const State &state) {
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
    broadcastStateUpdate();
  }
}

void HomeAssistantServer::notifyBlockedCall(const char *number) {
  Logger::infoln(F("Blocked call: %s"), number);
  _stats[3]++; // blocked calls
  broadcastStateUpdate();
}

void HomeAssistantServer::broadcastStateUpdate() {
  if (_ws.count() > 0) {
    // Send minimal state update via WebSocket
    JsonDocument doc;
    doc["state"] = appStateToString(_lastState.newAppState);
    doc["dnd"] = _lastState.isDnd;
    if (_lastState.callState.callNumber[0] != '\0') {
      doc["call"]["number"] = _lastState.callState.callNumber;
    }

    String response;
    serializeJson(doc, response);
    _ws.textAll(response);
  }
}

// Configuration management
void loadConfiguration() {
  if (!SPIFFS.exists(kConfigFile)) {
    seedHaPhoneBookFromLocal();
    haConfig.phoneBookInitialized = true;
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

  haConfig.dndForceEnabled = doc["dnd_force_enabled"] | false;
  haConfig.dndScheduleEnabled = doc["dnd_schedule_enabled"] | false;
  haConfig.dndStartHour = doc["dnd_start_hour"] | kDndStartHour;
  haConfig.dndStartMinute = doc["dnd_start_minute"] | kDndStartMinute;
  haConfig.dndEndHour = doc["dnd_end_hour"] | kDndEndHour;
  haConfig.dndEndMinute = doc["dnd_end_minute"] | kDndEndMinute;
  haConfig.phoneBookInitialized = doc["phonebook_initialized"] | false;
  haConfig.haServerUrl = doc["ha_server_url"] | "";
  haConfig.deviceName = doc["device_name"] | kHaDeviceName;

  // Load phonebook
  if (doc["phonebook"].is<JsonArray>()) {
    haConfig.phoneBook.clear();
    for (JsonObject entry : doc["phonebook"].as<JsonArray>()) {
      haConfig.phoneBook.push_back({entry["name"], entry["number"]});
    }
  }

  // Load blocked numbers
  if (doc["blocked_numbers"].is<JsonArray>()) {
    haConfig.blockedNumbers.clear();
    for (const String &number : doc["blocked_numbers"].as<JsonArray>()) {
      haConfig.blockedNumbers.push_back(number);
    }
  }

  // Load webhooks
  if (doc["webhooks"].is<JsonArray>()) {
    haConfig.webhooks.clear();
    for (JsonObject hook : doc["webhooks"].as<JsonArray>()) {
      WebhookEntry entry;
      entry.number = hook["number"].as<String>();
      entry.webhookId = hook["webhook_id"].as<String>();
      haConfig.webhooks.push_back(entry);
    }
  }

  if (haConfig.phoneBook.empty() && !haConfig.phoneBookInitialized) {
    seedHaPhoneBookFromLocal();
    haConfig.phoneBookInitialized = true;
    saveConfiguration();
  }

  file.close();
}

void saveConfiguration() {
  JsonDocument doc;

  doc["dnd_force_enabled"] = haConfig.dndForceEnabled;
  doc["dnd_schedule_enabled"] = haConfig.dndScheduleEnabled;
  doc["dnd_start_hour"] = haConfig.dndStartHour;
  doc["dnd_start_minute"] = haConfig.dndStartMinute;
  doc["dnd_end_hour"] = haConfig.dndEndHour;
  doc["dnd_end_minute"] = haConfig.dndEndMinute;
  doc["phonebook_initialized"] = haConfig.phoneBookInitialized;
  doc["ha_server_url"] = haConfig.haServerUrl;
  doc["device_name"] = haConfig.deviceName;

  JsonArray phonebook = doc["phonebook"].to<JsonArray>();
  for (const auto &entry : haConfig.phoneBook) {
    JsonObject entryObj = phonebook.add<JsonObject>();
    entryObj["name"] = entry.first;
    entryObj["number"] = entry.second;
  }

  JsonArray blocked = doc["blocked_numbers"].to<JsonArray>();
  for (const String &number : haConfig.blockedNumbers) {
    blocked.add(number);
  }

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

void seedHaPhoneBookFromLocal() {
  haConfig.phoneBook.clear();
  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; i++) {
    haConfig.phoneBook.push_back({phoneBookEntries[i].entry, phoneBookEntries[i].number});
  }
  saveConfiguration();
}

// Utility functions
bool isNumberBlocked(const char *number) {
  String numStr(number);
  return std::find(haConfig.blockedNumbers.begin(), haConfig.blockedNumbers.end(), numStr) !=
         haConfig.blockedNumbers.end();
}

bool isDndConfigEnabled() {
  return haConfig.dndForceEnabled || haConfig.dndScheduleEnabled;
}

bool isDndForceEnabled() {
  return haConfig.dndForceEnabled;
}

bool isDndScheduleEnabled() {
  return haConfig.dndScheduleEnabled;
}

void getHaDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute) {
  startHour = haConfig.dndStartHour;
  startMinute = haConfig.dndStartMinute;
  endHour = haConfig.dndEndHour;
  endMinute = haConfig.dndEndMinute;
}

bool isHaPhoneBookEntry(const char *number) {
  for (const auto &entry : haConfig.phoneBook) {
    if (entry.first == String(number)) {
      return true;
    }
  }
  return false;
}

bool isPartialOfHaPhoneBookEntry(const char *number) {
  String numStr(number);
  for (const auto &entry : haConfig.phoneBook) {
    if (entry.first.startsWith(numStr)) {
      return true;
    }
  }
  return false;
}

const char *getHaPhoneBookNumberForEntry(const char *entry) {
  static String cachedResult;
  for (const auto &phoneEntry : haConfig.phoneBook) {
    if (phoneEntry.first == String(entry)) {
      cachedResult = phoneEntry.second;
      return cachedResult.c_str();
    }
  }
  return nullptr;
}

// Webhook functions
bool isWebhookEntry(const char *number) {
  String numStr(number);
  for (const auto &hook : haConfig.webhooks) {
    if (hook.number == numStr) {
      return true;
    }
  }
  return false;
}

bool isPartialOfWebhookEntry(const char *number) {
  String numStr(number);
  for (const auto &hook : haConfig.webhooks) {
    if (hook.number.startsWith(numStr)) {
      return true;
    }
  }
  return false;
}

const char *getWebhookIdForNumber(const char *number) {
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

void executeWebhook(const char *webhookId) {
  if (haConfig.haServerUrl.length() == 0) {
    Logger::errorln(F("No HA server URL configured"));
    return;
  }

  HTTPClient http;
  String url = haConfig.haServerUrl + "/api/webhook/" + String(webhookId);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST("{}");

  if (httpResponseCode > 0) {
    Logger::infoln(F("Webhook executed: %s (response: %d)"), webhookId, httpResponseCode);
  } else {
    Logger::errorln(F("Webhook failed: %s (error: %d)"), webhookId, httpResponseCode);
  }

  http.end();
}

#endif // HOME_ASSISTANT_INTEGRATION
