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
#include <SPIFFS.h>
#include <WiFi.h>
#include <vector>

namespace {
  const constexpr int kHttpServerPort = 80;
  const constexpr char *kContentTypeJson = "application/json";
  const constexpr char *kContentTypeText = "text/plain";
  const constexpr char *kConfigFile = "/ha_config.json";
  const constexpr char *kPhoneBookFile = "/ha_phonebook.json";
  const constexpr char *kBlockedFile = "/ha_blocked.json";
  const constexpr int kJsonBufferSize = 2048;

  // Helper function to format uptime in human-readable format
  String formatUptime(unsigned long uptimeMs) {
    unsigned long seconds = uptimeMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    String result = "";
    if (days > 0) {
      result += String(days) + "d ";
    }
    if (hours > 0 || days > 0) {
      result += String(hours) + "h ";
    }
    if (minutes > 0 || hours > 0 || days > 0) {
      result += String(minutes) + "m ";
    }
    result += String(seconds) + "s";

    return result;
  }
}

// Storage for dynamic configuration
struct HaConfig {
  bool dndForceEnabled = false;    // Force DnD regardless of schedule
  bool dndScheduleEnabled = false; // Enable/disable schedule-based DnD
  int dndStartHour = kDndStartHour;
  int dndStartMinute = kDndStartMinute;
  int dndEndHour = kDndEndHour;
  int dndEndMinute = kDndEndMinute;
  std::vector<std::pair<String, String>> phoneBook;
  std::vector<String> blockedNumbers;
  bool phoneBookInitialized = false; // Track if phonebook has ever been user-managed
};

static HaConfig haConfig;

// Forward declarations
void loadConfiguration();
void saveConfiguration();
void seedHaPhoneBookFromLocal();

// Load configuration from SPIFFS
void loadConfiguration() {
  if (!SPIFFS.exists(kConfigFile)) {
    Logger::infoln(F("No HA config file found, using defaults and seeding phonebook"));
    // Seed with local phonebook on first run
    seedHaPhoneBookFromLocal();
    haConfig.phoneBookInitialized = true; // Mark as initialized after initial seeding
    saveConfiguration();                  // Save the configuration with the flag
    return;
  }

  File file = SPIFFS.open(kConfigFile, "r");
  if (!file) {
    Logger::errorln(F("Failed to open HA config file"));
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    Logger::errorln(F("Failed to parse HA config file"));
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

  // Load phonebook
  if (doc["phonebook"].is<JsonArray>()) {
    JsonArray phonebook = doc["phonebook"];
    haConfig.phoneBook.clear();
    for (JsonObject entry : phonebook) {
      String name = entry["name"];
      String number = entry["number"];
      haConfig.phoneBook.push_back({name, number});
    }
  }

  // Only seed HA phonebook if it's empty AND has never been user-managed
  if (haConfig.phoneBook.empty() && !haConfig.phoneBookInitialized) {
    Logger::infoln(
        F("HA phonebook empty and never initialized, seeding with local phonebook entries"));
    seedHaPhoneBookFromLocal();
    haConfig.phoneBookInitialized = true; // Mark as initialized after seeding
    saveConfiguration();                  // Save the updated flag
  }

  // Load blocked numbers
  if (doc["blocked_numbers"].is<JsonArray>()) {
    JsonArray blocked = doc["blocked_numbers"];
    haConfig.blockedNumbers.clear();
    for (const String &number : blocked) {
      haConfig.blockedNumbers.push_back(number);
    }
  }

  file.close();
  Logger::infoln(F("HA configuration loaded"));
}

// Save configuration to SPIFFS
void saveConfiguration() {
  JsonDocument doc;

  doc["dnd_force_enabled"] = haConfig.dndForceEnabled;
  doc["dnd_schedule_enabled"] = haConfig.dndScheduleEnabled;
  doc["dnd_start_hour"] = haConfig.dndStartHour;
  doc["dnd_start_minute"] = haConfig.dndStartMinute;
  doc["dnd_end_hour"] = haConfig.dndEndHour;
  doc["dnd_end_minute"] = haConfig.dndEndMinute;
  doc["phonebook_initialized"] = haConfig.phoneBookInitialized;

  // Save phonebook
  JsonArray phonebook = doc["phonebook"].to<JsonArray>();
  for (const auto &entry : haConfig.phoneBook) {
    JsonObject entryObj = phonebook.add<JsonObject>();
    entryObj["name"] = entry.first;
    entryObj["number"] = entry.second;
  }

  // Save blocked numbers
  JsonArray blocked = doc["blocked_numbers"].to<JsonArray>();
  for (const String &number : haConfig.blockedNumbers) {
    blocked.add(number);
  }

  File file = SPIFFS.open(kConfigFile, "w");
  if (!file) {
    Logger::errorln(F("Failed to create HA config file"));
    return;
  }

  if (serializeJson(doc, file) == 0) {
    Logger::errorln(F("Failed to write HA config file"));
  } else {
    Logger::infoln(F("HA configuration saved"));
  }

  file.close();
}

// Global instance
HomeAssistantServer haServer;

// Forward declarations for callback functions
extern void haPerformCall(const char *number);
extern void haPerformHangup();
extern void haPerformReset();
extern void haPerformRing(int durationMs);
extern void haPerformSetMaintenanceMode(bool enabled);
extern void haPerformSwitchToCallWaiting();
extern void haSetDndHours(int startHour, int startMinute, int endHour, int endMinute);

HomeAssistantServer::HomeAssistantServer()
    : _server(80),
      _ws("/ws"),
      _uptime(0),
      _totalCalls(0),
      _totalIncomingCalls(0),
      _totalOutgoingCalls(0),
      _totalBlockedCalls(0),
      _totalResets(0),
      _isInitialized(false),
      _stateChanged(false) {}

void HomeAssistantServer::init() {
  Logger::infoln(F("Initializing Home Assistant HTTP Server..."));

  if (!SPIFFS.begin(true)) {
    Logger::errorln(F("Failed to initialize SPIFFS"));
    return;
  }

  // Load configuration
  loadConfiguration();

  setupRoutes();
  setupWebSocket();
  _server.begin();

  // Setup mDNS
  if (MDNS.begin(kHaDeviceName)) {
    MDNS.addService("http", "tcp", kHttpServerPort);
    MDNS.addServiceTxt("http", "tcp", "model", "TsuryPhone");
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    Logger::infoln(F("mDNS responder started: %s.local"), kHaDeviceName);
  }

  _uptime = millis();
  _isInitialized = true;

  Logger::infoln(F("Home Assistant HTTP Server initialized on port %d"), kHttpServerPort);
}

void HomeAssistantServer::setupRoutes() {
  // Enable CORS for all endpoints
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET,POST,PUT,DELETE,OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // Root endpoint - device info
  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });

  // Status endpoint - current state
  _server.on(
      "/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStatus(request); });

  // Stats endpoint - statistics
  _server.on("/stats", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStats(request); });

  // Action endpoints
  _server.on(
      "/action/call",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handleAction(request, data, len, index, total);
      });

  _server.on("/action/hangup", HTTP_POST, [this](AsyncWebServerRequest *request) {
    haPerformHangup();
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Hangup initiated\"}");
    broadcastStateUpdate();
  });

  _server.on("/action/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Reset initiated\"}");
    broadcastStateUpdate();
    haPerformReset();
  });

  _server.on(
      "/action/ring",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handleRing(request, data, len, index, total);
      });

  _server.on(
      "/action/maintenance_mode",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handleMaintenanceMode(request, data, len, index, total);
      });

  _server.on("/action/switch_call_waiting", HTTP_POST, [this](AsyncWebServerRequest *request) {
    haPerformSwitchToCallWaiting();
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Switched to call waiting\"}");
    broadcastStateUpdate();
  });

  // DnD configuration
  _server.on("/dnd", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getDndConfigJson());
  });

  _server.on(
      "/dnd",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handleDnd(request, data, len, index, total);
      });

  // PhoneBook management
  _server.on("/phonebook", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getPhoneBookJson());
  });

  _server.on(
      "/phonebook",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handlePhoneBook(request, data, len, index, total);
      });

  // Blocked numbers management
  _server.on("/blocked", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getBlockedNumbersJson());
  });

  _server.on(
      "/blocked",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        handleBlockedNumbers(request, data, len, index, total);
      });

  // Handle CORS preflight
  _server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, kContentTypeJson, "{\"error\":\"Not found\"}");
    }
  });
}

void HomeAssistantServer::handleRoot(AsyncWebServerRequest *request) {
  JsonDocument doc;

  doc["device"]["name"] = kHaDeviceName;
  doc["device"]["model"] = "TsuryPhone";
  doc["device"]["version"] = "1.0";
  doc["device"]["manufacturer"] = "TsuryPhone Project";
  doc["device"]["ip"] = WiFi.localIP().toString();
  doc["device"]["mac"] = WiFi.macAddress();
  doc["device"]["uptime"] = formatUptime(millis());

  doc["endpoints"]["status"] = "/status";
  doc["endpoints"]["stats"] = "/stats";
  doc["endpoints"]["dnd"] = "/dnd";
  doc["endpoints"]["phonebook"] = "/phonebook";
  doc["endpoints"]["blocked"] = "/blocked";
  doc["endpoints"]["websocket"] = "/ws";
  doc["endpoints"]["actions"]["call"] = "/action/call";
  doc["endpoints"]["actions"]["hangup"] = "/action/hangup";
  doc["endpoints"]["actions"]["reset"] = "/action/reset";
  doc["endpoints"]["actions"]["ring"] = "/action/ring";
  doc["endpoints"]["actions"]["maintenance_mode"] = "/action/maintenance_mode";
  doc["endpoints"]["actions"]["switch_call_waiting"] = "/action/switch_call_waiting";

  String response;
  serializeJson(doc, response);
  sendJsonResponse(request, response);
}

void HomeAssistantServer::handleStatus(AsyncWebServerRequest *request) {
  sendJsonResponse(request, getStatusJson());
}

void HomeAssistantServer::handleAction(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (data == nullptr || len == 0) {
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  if (!doc["number"].is<String>()) {
    sendErrorResponse(request, "Missing 'number' field");
    return;
  }

  String number = doc["number"];

  if (number.length() == 0) {
    sendErrorResponse(request, "Empty number provided");
    return;
  }

  haPerformCall(number.c_str());
  _totalOutgoingCalls++;
  sendJsonResponse(request, "{\"success\":true,\"message\":\"Call initiated to " + number + "\"}");

  // Broadcast state change immediately
  broadcastStateUpdate();
}

void HomeAssistantServer::handleRing(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (data == nullptr || len == 0) {
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  if (!doc["duration"].is<int>()) {
    sendErrorResponse(request, "Missing 'duration' field");
    return;
  }

  int duration = doc["duration"];
  if (duration <= 0 || duration > 30000) { // Max 30 seconds
    sendErrorResponse(request, "Duration must be between 1 and 30000 ms");
    return;
  }

  haPerformRing(duration);
  sendJsonResponse(
      request, "{\"success\":true,\"message\":\"Ring initiated for " + String(duration) + " ms\"}");
}

void HomeAssistantServer::handleMaintenanceMode(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (data == nullptr || len == 0) {
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  if (!doc["enabled"].is<bool>()) {
    sendErrorResponse(request, "Missing 'enabled' field");
    return;
  }

  bool enabled = doc["enabled"];

  haPerformSetMaintenanceMode(enabled);
  sendJsonResponse(request,
                   "{\"success\":true,\"message\":\"Maintenance mode " +
                       String(enabled ? "enabled" : "disabled") + "\"}");
}

void HomeAssistantServer::handleDnd(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (request->method() == HTTP_GET) {
    sendJsonResponse(request, getDndConfigJson());
    return;
  }

  // Handle POST with JSON body
  if (data == nullptr || len == 0) {
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  // Handle force_enabled parameter
  if (doc["force_enabled"].is<bool>()) {
    bool forceEnabled = doc["force_enabled"];
    haConfig.dndForceEnabled = forceEnabled;
  }

  // Handle schedule_enabled parameter
  if (doc["schedule_enabled"].is<bool>()) {
    bool scheduleEnabled = doc["schedule_enabled"];
    haConfig.dndScheduleEnabled = scheduleEnabled;
  }

  // Handle start_time and end_time parameters (HH:MM format)
  if (doc["start_time"].is<String>() && doc["end_time"].is<String>()) {
    String startTimeStr = doc["start_time"];
    String endTimeStr = doc["end_time"];

    int startHour, startMinute, endHour, endMinute;

    // Parse start time (HH:MM format)
    int colonPos = startTimeStr.indexOf(':');
    if (colonPos > 0 && colonPos < startTimeStr.length() - 1) {
      int parsedStartHour = startTimeStr.substring(0, colonPos).toInt();
      int parsedStartMinute = startTimeStr.substring(colonPos + 1).toInt();

      // Validate parsed start time
      if (parsedStartHour >= 0 && parsedStartHour < 24 && parsedStartMinute >= 0 &&
          parsedStartMinute < 60) {
        startHour = parsedStartHour;
        startMinute = parsedStartMinute;
      } else {
        sendErrorResponse(request, "Invalid start_time format or values");
        return;
      }
    } else {
      sendErrorResponse(request, "Invalid start_time format (expected HH:MM)");
      return;
    }

    // Parse end time (HH:MM format)
    colonPos = endTimeStr.indexOf(':');
    if (colonPos > 0 && colonPos < endTimeStr.length() - 1) {
      int parsedEndHour = endTimeStr.substring(0, colonPos).toInt();
      int parsedEndMinute = endTimeStr.substring(colonPos + 1).toInt();

      // Validate parsed end time
      if (parsedEndHour >= 0 && parsedEndHour < 24 && parsedEndMinute >= 0 &&
          parsedEndMinute < 60) {
        endHour = parsedEndHour;
        endMinute = parsedEndMinute;
      } else {
        sendErrorResponse(request, "Invalid end_time format or values");
        return;
      }
    } else {
      sendErrorResponse(request, "Invalid end_time format (expected HH:MM)");
      return;
    }

    // Apply validated time settings
    haConfig.dndStartHour = startHour;
    haConfig.dndStartMinute = startMinute;
    haConfig.dndEndHour = endHour;
    haConfig.dndEndMinute = endMinute;

    haSetDndHours(startHour, startMinute, endHour, endMinute);
  }

  // Handle individual hour/minute parameters for compatibility with HA number entities
  if (doc["start_hour"].is<int>() || doc["start_minute"].is<int>() || doc["end_hour"].is<int>() ||
      doc["end_minute"].is<int>()) {

    int startHour = doc["start_hour"].is<int>() ? doc["start_hour"] : haConfig.dndStartHour;
    int startMinute = doc["start_minute"].is<int>() ? doc["start_minute"] : haConfig.dndStartMinute;
    int endHour = doc["end_hour"].is<int>() ? doc["end_hour"] : haConfig.dndEndHour;
    int endMinute = doc["end_minute"].is<int>() ? doc["end_minute"] : haConfig.dndEndMinute;

    // Validate time values
    if (startHour >= 0 && startHour < 24 && startMinute >= 0 && startMinute < 60 && endHour >= 0 &&
        endHour < 24 && endMinute >= 0 && endMinute < 60) {

      haConfig.dndStartHour = startHour;
      haConfig.dndStartMinute = startMinute;
      haConfig.dndEndHour = endHour;
      haConfig.dndEndMinute = endMinute;

      haSetDndHours(startHour, startMinute, endHour, endMinute);
    } else {
      sendErrorResponse(request, "Invalid hour or minute values");
      return;
    }
  }

  saveConfiguration();
  sendJsonResponse(request, getDndConfigJson());

  // Broadcast configuration change
  broadcastStateUpdate();
}

void HomeAssistantServer::handlePhoneBook(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (request->method() == HTTP_GET) {
    sendJsonResponse(request, getPhoneBookJson());
    return;
  }

  // Handle POST with JSON body
  if (data == nullptr || len == 0) {
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  String action = doc["action"] | "add";

  if (action == "add") {
    if (!doc["name"].is<String>() || !doc["number"].is<String>()) {
      sendErrorResponse(request, "Missing 'name' or 'number' field");
      return;
    }

    String name = doc["name"];
    String number = doc["number"];

    // Remove existing entry with same name
    haConfig.phoneBook.erase(std::remove_if(haConfig.phoneBook.begin(),
                                            haConfig.phoneBook.end(),
                                            [&name](const std::pair<String, String> &entry) {
                                              return entry.first == name;
                                            }),
                             haConfig.phoneBook.end());

    // Add new entry
    haConfig.phoneBook.push_back({name, number});
    haConfig.phoneBookInitialized = true; // Mark as user-managed
    saveConfiguration();

    sendJsonResponse(
        request, "{\"success\":true,\"message\":\"Entry added: " + name + " -> " + number + "\"}");

    // Broadcast phonebook change
    broadcastStateUpdate();

  } else if (action == "remove") {
    if (!doc["name"].is<String>()) {
      sendErrorResponse(request, "Missing 'name' field");
      return;
    }

    String name = doc["name"];
    size_t originalSize = haConfig.phoneBook.size();

    haConfig.phoneBook.erase(std::remove_if(haConfig.phoneBook.begin(),
                                            haConfig.phoneBook.end(),
                                            [&name](const std::pair<String, String> &entry) {
                                              return entry.first == name;
                                            }),
                             haConfig.phoneBook.end());

    if (haConfig.phoneBook.size() < originalSize) {
      haConfig.phoneBookInitialized = true; // Mark as user-managed
      saveConfiguration();
      sendJsonResponse(request, "{\"success\":true,\"message\":\"Entry removed: " + name + "\"}");
      broadcastStateUpdate();
    } else {
      sendErrorResponse(request, "Entry not found: " + name, 404);
    }
  } else {
    sendErrorResponse(request, "Invalid action. Use 'add' or 'remove'");
  }
}

void HomeAssistantServer::handleBlockedNumbers(
    AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  Logger::infoln(F("Received blocked numbers request: method=%d"), request->method());

  if (request->method() == HTTP_GET) {
    Logger::infoln(F("Returning blocked numbers list"));
    sendJsonResponse(request, getBlockedNumbersJson());
    return;
  }

  // Handle POST with JSON body
  if (data == nullptr || len == 0) {
    Logger::errorln(F("Blocked POST: missing JSON body"));
    sendErrorResponse(request, "Missing JSON body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Logger::errorln(F("Blocked POST: JSON parse error: %s"), error.c_str());
    sendErrorResponse(request, "Invalid JSON format");
    return;
  }

  String action = doc["action"] | "add";
  Logger::infoln(F("Blocked numbers POST: action=%s"), action.c_str());

  if (action == "add") {
    if (!doc["number"].is<String>()) {
      Logger::errorln(F("Blocked add: missing number field"));
      sendErrorResponse(request, "Missing 'number' field");
      return;
    }

    String number = doc["number"];
    Logger::infoln(F("Adding blocked number: %s"), number.c_str());

    // Check if already exists
    if (std::find(haConfig.blockedNumbers.begin(), haConfig.blockedNumbers.end(), number) ==
        haConfig.blockedNumbers.end()) {
      haConfig.blockedNumbers.push_back(number);
      saveConfiguration();
      Logger::infoln(F("Successfully added blocked number: %s"), number.c_str());
      sendJsonResponse(request,
                       "{\"success\":true,\"message\":\"Blocked number added: " + number + "\"}");
      broadcastStateUpdate();
    } else {
      Logger::warnln(F("Blocked number already exists: %s"), number.c_str());
      sendErrorResponse(request, "Number already blocked: " + number, 409);
    }

  } else if (action == "remove") {
    if (!doc["number"].is<String>()) {
      Logger::errorln(F("Blocked remove: missing number field"));
      sendErrorResponse(request, "Missing 'number' field");
      return;
    }

    String number = doc["number"];
    Logger::infoln(F("Removing blocked number: %s"), number.c_str());

    auto it = std::find(haConfig.blockedNumbers.begin(), haConfig.blockedNumbers.end(), number);

    if (it != haConfig.blockedNumbers.end()) {
      haConfig.blockedNumbers.erase(it);
      saveConfiguration();
      Logger::infoln(F("Successfully removed blocked number: %s"), number.c_str());
      sendJsonResponse(request,
                       "{\"success\":true,\"message\":\"Blocked number removed: " + number + "\"}");
      broadcastStateUpdate();
    } else {
      Logger::warnln(F("Blocked number not found: %s"), number.c_str());
      sendErrorResponse(request, "Number not found: " + number, 404);
    }
  } else {
    Logger::errorln(F("Invalid blocked action: %s"), action.c_str());
    sendErrorResponse(request, "Invalid action. Use 'add' or 'remove'");
  }
}

void HomeAssistantServer::handleStats(AsyncWebServerRequest *request) {
  sendJsonResponse(request, getStatsJson());
}

String HomeAssistantServer::getStatusJson() {
  JsonDocument doc;

  doc["state"] = appStateToString(_lastState.newAppState);
  doc["previous_state"] = appStateToString(_lastState.prevAppState);
  doc["dnd_enabled"] = _lastState.isDnd;
  doc["maintenance_mode"] = _lastState.isMaintenanceMode;
  doc["uptime"] = formatUptime(millis());
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi"]["connected"] = WiFi.isConnected();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["ssid"] = WiFi.SSID();

  if (_lastState.callState.callId != -1) {
    doc["call"]["active"] = true;
    doc["call"]["id"] = _lastState.callState.callId;
    doc["call"]["number"] = _lastState.callState.callNumber;
    doc["call"]["has_waiting"] = _lastState.callState.hasCallWaiting();
    doc["call"]["has_call_waiting"] =
        _lastState.callState.hasCallWaiting(); // For HA integration compatibility
    if (_lastState.callState.hasCallWaiting()) {
      doc["call"]["waiting_id"] = _lastState.callState.callWaitingId;
    }
  } else {
    doc["call"]["active"] = false;
  }

  String response;
  serializeJson(doc, response);
  return response;
}

String HomeAssistantServer::getStatsJson() {
  JsonDocument doc;

  doc["uptime"] = formatUptime(millis());
  doc["total_calls"] = _totalCalls;
  doc["total_incoming_calls"] = _totalIncomingCalls;
  doc["total_outgoing_calls"] = _totalOutgoingCalls;
  doc["total_blocked_calls"] = _totalBlockedCalls;
  doc["total_resets"] = _totalResets;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["heap_size"] = ESP.getHeapSize();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["sketch_size"] = ESP.getSketchSize();
  doc["free_sketch_space"] = ESP.getFreeSketchSpace();
  doc["sdk_version"] = ESP.getSdkVersion();
  doc["cpu_freq"] = ESP.getCpuFreqMHz();
  doc["chip_revision"] = ESP.getChipRevision();
  doc["chip_model"] = ESP.getChipModel();
  doc["chip_cores"] = ESP.getChipCores();

  String response;
  serializeJson(doc, response);
  return response;
}

String HomeAssistantServer::getPhoneBookJson() {
  JsonDocument doc;

  JsonArray entries = doc["entries"].to<JsonArray>();
  for (const auto &entry : haConfig.phoneBook) {
    JsonObject entryObj = entries.add<JsonObject>();
    entryObj["name"] = entry.first;
    entryObj["number"] = entry.second;
  }

  String response;
  serializeJson(doc, response);
  return response;
}

String HomeAssistantServer::getBlockedNumbersJson() {
  JsonDocument doc;

  JsonArray numbers = doc["blocked_numbers"].to<JsonArray>();
  for (const String &number : haConfig.blockedNumbers) {
    numbers.add(number);
  }

  String response;
  serializeJson(doc, response);
  return response;
}

String HomeAssistantServer::getDndConfigJson() {
  JsonDocument doc;

  doc["force_enabled"] = haConfig.dndForceEnabled;
  doc["schedule_enabled"] = haConfig.dndScheduleEnabled;

  // Time format (HH:MM)
  char timeBuffer[6]; // HH:MM\0
  snprintf(
      timeBuffer, sizeof(timeBuffer), "%02d:%02d", haConfig.dndStartHour, haConfig.dndStartMinute);
  doc["start_time"] = timeBuffer;

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", haConfig.dndEndHour, haConfig.dndEndMinute);
  doc["end_time"] = timeBuffer;

  // Individual time components for compatibility with HA number entities
  doc["start_hour"] = haConfig.dndStartHour;
  doc["start_minute"] = haConfig.dndStartMinute;
  doc["end_hour"] = haConfig.dndEndHour;
  doc["end_minute"] = haConfig.dndEndMinute;

  doc["currently_active"] = _lastState.isDnd;

  String response;
  serializeJson(doc, response);
  return response;
}

void HomeAssistantServer::sendJsonResponse(AsyncWebServerRequest *request,
                                           const String &json,
                                           int code) {
  request->send(code, kContentTypeJson, json);
}

void HomeAssistantServer::sendErrorResponse(AsyncWebServerRequest *request,
                                            const String &error,
                                            int code) {
  JsonDocument doc;
  doc["error"] = error;
  String response;
  serializeJson(doc, response);
  sendJsonResponse(request, response, code);
}

void HomeAssistantServer::process() {
  if (!_isInitialized) {
    return;
  }

  // Clean up disconnected WebSocket clients periodically
  static uint32_t lastCleanup = 0;
  if (millis() - lastCleanup > 30000) { // Every 30 seconds
    _ws.cleanupClients();
    lastCleanup = millis();
  }

  // Update statistics periodically
  static uint32_t lastStatsUpdate = 0;
  if (millis() - lastStatsUpdate > 30000) { // Every 30 seconds
    lastStatsUpdate = millis();
    // Could add periodic tasks here
  }
}

void HomeAssistantServer::updateState(const State &state) {
  AppState prevState = _lastState.newAppState;
  bool stateChanged = false;

  // Check if state actually changed
  if (_lastState.newAppState != state.newAppState || _lastState.isDnd != state.isDnd ||
      _lastState.isMaintenanceMode != state.isMaintenanceMode ||
      _lastState.callState.callId != state.callState.callId ||
      strcmp(_lastState.callState.callNumber, state.callState.callNumber) != 0) {
    stateChanged = true;
  }

  _lastState = state;

  // Track call statistics
  if (prevState != AppState::InCall && state.newAppState == AppState::InCall) {
    _totalCalls++;
    stateChanged = true;
  }

  if (prevState != AppState::IncomingCall && state.newAppState == AppState::IncomingCall) {
    _totalIncomingCalls++;
    stateChanged = true;
  }

  // Broadcast state changes via WebSocket
  if (stateChanged) {
    broadcastStateUpdate();
  }
}

void HomeAssistantServer::notifyBlockedCall(const char *number) {
  Logger::infoln(F("Notifying HA about blocked call from: %s"), number);
  _totalBlockedCalls++;

  // Broadcast the updated stats immediately via WebSocket
  broadcastStateUpdate();
}

// WebSocket setup and event handling
void HomeAssistantServer::setupWebSocket() {
  _ws.onEvent([this](AsyncWebSocket *server,
                     AsyncWebSocketClient *client,
                     AwsEventType type,
                     void *arg,
                     uint8_t *data,
                     size_t len) { this->onWebSocketEvent(server, client, type, arg, data, len); });
  _server.addHandler(&_ws);
  Logger::infoln(F("WebSocket server configured on /ws"));
}

void HomeAssistantServer::onWebSocketEvent(AsyncWebSocket *server,
                                           AsyncWebSocketClient *client,
                                           AwsEventType type,
                                           void *arg,
                                           uint8_t *data,
                                           size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    Logger::infoln(F("WebSocket client connected: %u"), client->id());
    // Send current state immediately to new client
    client->text(getStatusJson());
    break;

  case WS_EVT_DISCONNECT:
    Logger::infoln(F("WebSocket client disconnected: %u"), client->id());
    break;

  case WS_EVT_DATA: {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      // Handle WebSocket messages if needed (for bidirectional communication)
      data[len] = 0; // Null terminate
      Logger::debugln(F("WebSocket message from %u: %s"), client->id(), (char *)data);
    }
    break;
  }

  case WS_EVT_PING:
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void HomeAssistantServer::broadcastStateUpdate() {
  if (_ws.count() > 0) {
    String statusJson = getStatusJson();
    _ws.textAll(statusJson);
    Logger::debugln(F("Broadcasted state update to %d WebSocket clients"), _ws.count());
  }
}

// Utility functions for other components to check HA configuration
bool isNumberBlocked(const char *number) {
  String numStr(number);
  return std::find(haConfig.blockedNumbers.begin(), haConfig.blockedNumbers.end(), numStr) !=
         haConfig.blockedNumbers.end();
}

bool isDndConfigEnabled() {
  // DnD is enabled if either force is enabled OR schedule is enabled
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

// HA Phonebook functions for runtime selection
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
  static String cachedResult; // Static to persist the result

  for (const auto &phoneEntry : haConfig.phoneBook) {
    if (phoneEntry.first == String(entry)) {
      cachedResult = phoneEntry.second;
      return cachedResult.c_str();
    }
  }
  return nullptr;
}

// Seed HA phonebook with local generated phonebook entries
void seedHaPhoneBookFromLocal() {
  Logger::infoln(F("Seeding HA phonebook with local generated entries..."));

  // Clear any existing entries
  haConfig.phoneBook.clear();

  // Add all entries from the generated phonebook
  size_t numEntries = sizeof(phoneBookEntries) / sizeof(phoneBookEntries[0]);
  for (size_t i = 0; i < numEntries; i++) {
    String entry = phoneBookEntries[i].entry;
    String number = phoneBookEntries[i].number;

    haConfig.phoneBook.push_back({entry, number});
    Logger::debugln(F("Seeded: %s -> %s"), entry.c_str(), number.c_str());
  }

  Logger::infoln(F("Seeded %d phonebook entries from local to HA"), numEntries);

  // Save the seeded phonebook to persistent storage
  saveConfiguration();
}

#endif // HOME_ASSISTANT_INTEGRATION
