#ifdef HOME_ASSISTANT_INTEGRATION

#include "homeAssistantServer.h"
#include "../generated/phoneBook.h"
#include "logger.h"
#include "phoneBook.h"
#include "state.h"
#include "string.h"
#include <ArduinoJson.h>
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
  const constexpr char *kScreenedFile = "/ha_screened.json";
  const constexpr int kJsonBufferSize = 2048;
}

// Storage for dynamic configuration
struct HaConfig {
  bool dndEnabled = false;
  int dndStartHour = kDndStartHour;
  int dndStartMinute = kDndStartMinute;
  int dndEndHour = kDndEndHour;
  int dndEndMinute = kDndEndMinute;
  std::vector<std::pair<String, String>> phoneBook;
  std::vector<String> screenedNumbers;
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

  haConfig.dndEnabled = doc["dnd_enabled"] | kDndEnabled;
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

  // Load screened numbers
  if (doc["screened_numbers"].is<JsonArray>()) {
    JsonArray screened = doc["screened_numbers"];
    haConfig.screenedNumbers.clear();
    for (const String &number : screened) {
      haConfig.screenedNumbers.push_back(number);
    }
  }

  file.close();
  Logger::infoln(F("HA configuration loaded"));
}

// Save configuration to SPIFFS
void saveConfiguration() {
  JsonDocument doc;

  doc["dnd_enabled"] = haConfig.dndEnabled;
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

  // Save screened numbers
  JsonArray screened = doc["screened_numbers"].to<JsonArray>();
  for (const String &number : haConfig.screenedNumbers) {
    screened.add(number);
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
extern void haSetDndEnabled(bool enabled);
extern void haSetDndHours(int startHour, int startMinute, int endHour, int endMinute);

void HomeAssistantServer::init() {
  Logger::infoln(F("Initializing Home Assistant HTTP Server..."));

  if (!SPIFFS.begin(true)) {
    Logger::errorln(F("Failed to initialize SPIFFS"));
    return;
  }

  // Load configuration
  loadConfiguration();

  setupRoutes();
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
      "/action/call", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAction(request); });

  _server.on("/action/hangup", HTTP_POST, [this](AsyncWebServerRequest *request) {
    haPerformHangup();
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Hangup initiated\"}");
  });

  _server.on("/action/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    haPerformReset();
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Reset initiated\"}");
  });

  _server.on("/action/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, "{\"success\":true,\"message\":\"Reboot initiated\"}");
    delay(1000);
    ESP.restart();
  });

  _server.on("/action/ring", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!request->hasParam("duration", true)) {
      sendErrorResponse(request, "Missing 'duration' parameter");
      return;
    }

    int duration = request->getParam("duration", true)->value().toInt();
    if (duration <= 0 || duration > 30000) { // Max 30 seconds
      sendErrorResponse(request, "Duration must be between 1 and 30000 ms");
      return;
    }

    haPerformRing(duration);
    sendJsonResponse(request,
                     "{\"success\":true,\"message\":\"Ring initiated for " + String(duration) +
                         " ms\"}");
  });

  // DnD configuration
  _server.on("/dnd", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getDndConfigJson());
  });

  _server.on("/dnd", HTTP_POST, [this](AsyncWebServerRequest *request) { handleDnd(request); });

  // PhoneBook management
  _server.on("/phonebook", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getPhoneBookJson());
  });

  _server.on("/phonebook", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handlePhoneBook(request);
  });

  // Screened numbers management
  _server.on("/screened", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendJsonResponse(request, getScreenedNumbersJson());
  });

  _server.on("/screened", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleScreenedNumbers(request);
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
  doc["device"]["uptime"] = millis();

  doc["endpoints"]["status"] = "/status";
  doc["endpoints"]["stats"] = "/stats";
  doc["endpoints"]["dnd"] = "/dnd";
  doc["endpoints"]["phonebook"] = "/phonebook";
  doc["endpoints"]["screened"] = "/screened";
  doc["endpoints"]["actions"]["call"] = "/action/call";
  doc["endpoints"]["actions"]["hangup"] = "/action/hangup";
  doc["endpoints"]["actions"]["reset"] = "/action/reset";
  doc["endpoints"]["actions"]["reboot"] = "/action/reboot";
  doc["endpoints"]["actions"]["ring"] = "/action/ring";

  String response;
  serializeJson(doc, response);
  sendJsonResponse(request, response);
}

void HomeAssistantServer::handleStatus(AsyncWebServerRequest *request) {
  sendJsonResponse(request, getStatusJson());
}

void HomeAssistantServer::handleAction(AsyncWebServerRequest *request) {
  if (!request->hasParam("number", true)) {
    sendErrorResponse(request, "Missing 'number' parameter");
    return;
  }

  String number = request->getParam("number", true)->value();

  if (number.length() == 0) {
    sendErrorResponse(request, "Empty number provided");
    return;
  }

  haPerformCall(number.c_str());
  _totalOutgoingCalls++;
  sendJsonResponse(request, "{\"success\":true,\"message\":\"Call initiated to " + number + "\"}");
}

void HomeAssistantServer::handleDnd(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    sendJsonResponse(request, getDndConfigJson());
    return;
  }

  // Handle POST - update DnD settings
  if (request->hasParam("enabled", true)) {
    bool enabled = request->getParam("enabled", true)->value() == "true";
    haConfig.dndEnabled = enabled;
    haSetDndEnabled(enabled);
  }

  if (request->hasParam("start_hour", true) && request->hasParam("start_minute", true) &&
      request->hasParam("end_hour", true) && request->hasParam("end_minute", true)) {

    int startHour = request->getParam("start_hour", true)->value().toInt();
    int startMinute = request->getParam("start_minute", true)->value().toInt();
    int endHour = request->getParam("end_hour", true)->value().toInt();
    int endMinute = request->getParam("end_minute", true)->value().toInt();

    if (startHour >= 0 && startHour < 24 && endHour >= 0 && endHour < 24 && startMinute >= 0 &&
        startMinute < 60 && endMinute >= 0 && endMinute < 60) {

      haConfig.dndStartHour = startHour;
      haConfig.dndStartMinute = startMinute;
      haConfig.dndEndHour = endHour;
      haConfig.dndEndMinute = endMinute;

      haSetDndHours(startHour, startMinute, endHour, endMinute);
    } else {
      sendErrorResponse(request, "Invalid time values");
      return;
    }
  }

  saveConfiguration();
  sendJsonResponse(request, getDndConfigJson());
}

void HomeAssistantServer::handlePhoneBook(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    sendJsonResponse(request, getPhoneBookJson());
    return;
  }

  // Handle POST - add/remove phonebook entry
  String action =
      request->hasParam("action", true) ? request->getParam("action", true)->value() : "add";

  if (action == "add") {
    if (!request->hasParam("name", true) || !request->hasParam("number", true)) {
      sendErrorResponse(request, "Missing 'name' or 'number' parameter");
      return;
    }

    String name = request->getParam("name", true)->value();
    String number = request->getParam("number", true)->value();

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

  } else if (action == "remove") {
    if (!request->hasParam("name", true)) {
      sendErrorResponse(request, "Missing 'name' parameter");
      return;
    }

    String name = request->getParam("name", true)->value();
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
    } else {
      sendErrorResponse(request, "Entry not found: " + name, 404);
    }
  } else {
    sendErrorResponse(request, "Invalid action. Use 'add' or 'remove'");
  }
}

void HomeAssistantServer::handleScreenedNumbers(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    sendJsonResponse(request, getScreenedNumbersJson());
    return;
  }

  // Handle POST - add/remove screened number
  String action =
      request->hasParam("action", true) ? request->getParam("action", true)->value() : "add";

  if (action == "add") {
    if (!request->hasParam("number", true)) {
      sendErrorResponse(request, "Missing 'number' parameter");
      return;
    }

    String number = request->getParam("number", true)->value();

    // Check if already exists
    if (std::find(haConfig.screenedNumbers.begin(), haConfig.screenedNumbers.end(), number) ==
        haConfig.screenedNumbers.end()) {
      haConfig.screenedNumbers.push_back(number);
      saveConfiguration();
      sendJsonResponse(request,
                       "{\"success\":true,\"message\":\"Screened number added: " + number + "\"}");
    } else {
      sendErrorResponse(request, "Number already screened: " + number, 409);
    }

  } else if (action == "remove") {
    if (!request->hasParam("number", true)) {
      sendErrorResponse(request, "Missing 'number' parameter");
      return;
    }

    String number = request->getParam("number", true)->value();
    auto it = std::find(haConfig.screenedNumbers.begin(), haConfig.screenedNumbers.end(), number);

    if (it != haConfig.screenedNumbers.end()) {
      haConfig.screenedNumbers.erase(it);
      saveConfiguration();
      sendJsonResponse(
          request, "{\"success\":true,\"message\":\"Screened number removed: " + number + "\"}");
    } else {
      sendErrorResponse(request, "Number not found: " + number, 404);
    }
  } else {
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
  doc["dnd_config_enabled"] = haConfig.dndEnabled;
  doc["uptime"] = millis();
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

  doc["uptime"] = millis();
  doc["total_calls"] = _totalCalls;
  doc["total_incoming_calls"] = _totalIncomingCalls;
  doc["total_outgoing_calls"] = _totalOutgoingCalls;
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

String HomeAssistantServer::getScreenedNumbersJson() {
  JsonDocument doc;

  JsonArray numbers = doc["screened_numbers"].to<JsonArray>();
  for (const String &number : haConfig.screenedNumbers) {
    numbers.add(number);
  }

  String response;
  serializeJson(doc, response);
  return response;
}

String HomeAssistantServer::getDndConfigJson() {
  JsonDocument doc;

  doc["enabled"] = haConfig.dndEnabled;
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

  // Update statistics periodically
  static uint32_t lastStatsUpdate = 0;
  if (millis() - lastStatsUpdate > 30000) { // Every 30 seconds
    lastStatsUpdate = millis();
    // Could add periodic tasks here
  }
}

void HomeAssistantServer::updateState(const State &state) {
  AppState prevState = _lastState.newAppState;
  _lastState = state;

  // Track call statistics
  if (prevState != AppState::InCall && state.newAppState == AppState::InCall) {
    _totalCalls++;
  }

  if (prevState != AppState::IncomingCall && state.newAppState == AppState::IncomingCall) {
    _totalIncomingCalls++;
  }
}

// Utility functions for other components to check HA configuration
bool isNumberScreened(const char *number) {
  String numStr(number);
  return std::find(haConfig.screenedNumbers.begin(), haConfig.screenedNumbers.end(), numStr) !=
         haConfig.screenedNumbers.end();
}

bool isDndConfigEnabled() {
  return haConfig.dndEnabled;
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
