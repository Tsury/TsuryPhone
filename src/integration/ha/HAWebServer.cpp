#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAWebServer.h"
#include "../../common/logger.h"
#include "../../common/phoneNormalization.h"
#include "../../common/state.h"
#include "../../common/timeManager.h"
#include "../../config.h"
#include "../../core/DeviceConfig.h"
#include "../IntegrationService.h" // For INTEGRATION_EVENT_SCHEMA_VERSION
#include "../Validation.h"
#include "../core/DeviceStats.h"
#include "../core/Diagnostics.h" // Generic diagnostics builder
#include <ESPmDNS.h>

const char *HAWebServer::kWebSocketPath = "/ws";

HAWebServer::HAWebServer(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config),
      _stats(stats),
      _state(state),
      _server(kServerPort),
      _webSocket(kWebSocketPath) {}

bool HAWebServer::init() {
  Logger::infoln(F("Initializing HA Web Server on port %d..."), kServerPort);

  setupRoutes();
  setupWebSocket();
  setupMDNS();

  _server.begin();

  Logger::infoln(F("HA Web Server started successfully"));

  return true;
}

void HAWebServer::process() {
  unsigned long now = millis();

  if (now - _lastCleanupTime >= kWebSocketCleanupInterval) {
    _webSocket.cleanupClients();
    _lastCleanupTime = now;
  }
}

void HAWebServer::stop() {
  _webSocket.closeAll();
  _server.end();
  MDNS.end();
}

void HAWebServer::setupRoutes() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  _server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  // GET routes
  _server.on("/api/config/tsuryphone", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetTsuryPhoneConfig(request);
  });

  _server.on("/api/refetch_all", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRefetchAll(request);
  });

  _server.on("/api/diagnostics", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleDiagnostics(request);
  });

  // Simple POST routes (no JSON body)
  _server.on("/api/call/answer", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleAnswerCall(request);
  });

  _server.on("/api/call/hangup", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleHangupCall(request);
  });

  _server.on("/api/call/switch_call_waiting", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleToggleCallWaiting(request);
  });

  _server.on("/api/system/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleResetDevice(request);
  });

  _server.on("/api/system/factory_reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleFactoryReset(request);
  });

  // JSON POST routes using helper function
  addJsonPostRoute("/api/call/dial", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleDialNumber(req, json);
  });

  addJsonPostRoute("/api/call/dial_digit", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleDialDigit(req, json);
  });

  addJsonPostRoute(
      "/api/call/dial_quick_dial",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleDialQuickDial(req, json); });

  addJsonPostRoute("/api/call/volume_mode", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleSetVolumeMode(req, json);
  });

  addJsonPostRoute("/api/config/dnd", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleSetDND(req, json);
  });

  addJsonPostRoute("/api/config/maintenance",
                   [this](AsyncWebServerRequest *req, JsonVariant &json) {
                     handleSetMaintenanceMode(req, json);
                   });

  addJsonPostRoute("/api/config/audio", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleSetAudioConfig(req, json);
  });

  addJsonPostRoute(
      "/api/config/ring_pattern",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleSetRingPattern(req, json); });

  addJsonPostRoute("/api/config/dialing", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleSetDialingConfig(req, json);
  });

  addJsonPostRoute(
      "/api/config/quick_dial_add",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleAddQuickDial(req, json); });

  addJsonPostRoute(
      "/api/config/quick_dial_remove",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleRemoveQuickDial(req, json); });

  addJsonPostRoute(
      "/api/config/webhook_add",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleAddWebhookAction(req, json); });

  addJsonPostRoute("/api/config/webhook_remove",
                   [this](AsyncWebServerRequest *req, JsonVariant &json) {
                     handleRemoveWebhookAction(req, json);
                   });

  addJsonPostRoute(
      "/api/config/blocked_add",
      [this](AsyncWebServerRequest *req, JsonVariant &json) { handleAddBlockedNumber(req, json); });

  addJsonPostRoute("/api/config/blocked_remove",
                   [this](AsyncWebServerRequest *req, JsonVariant &json) {
                     handleRemoveBlockedNumber(req, json);
                   });

  addJsonPostRoute("/api/system/ring", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleRingOperation(req, json);
  });

  // Priority callers
  addJsonPostRoute("/api/config/priority_add",
                   [this](AsyncWebServerRequest *req, JsonVariant &json) {
                     handleAddPriorityCaller(req, json);
                   });
  addJsonPostRoute("/api/config/priority_remove",
                   [this](AsyncWebServerRequest *req, JsonVariant &json) {
                     handleRemovePriorityCaller(req, json);
                   });

  addJsonPostRoute("/api/config/ha_url", [this](AsyncWebServerRequest *req, JsonVariant &json) {
    handleSetHAUrl(req, json);
  });
}

void HAWebServer::addJsonPostRoute(
    const String &path, std::function<void(AsyncWebServerRequest *, JsonVariant &)> handler) {
  _server.on(
      path.c_str(),
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {},
      NULL,
      [this, handler](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;

        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handler(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON", 400, "WEB_INVALID_JSON");
        }
      });
}

void HAWebServer::executeCommand(AsyncWebServerRequest *request,
                                 const String &command,
                                 const JsonVariant &data) {
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback(command, data);

    IntegrationService &svc = IntegrationService::shared(_config, _stats, _state);
    if (result.success) {
      JsonDocument doc = svc.buildSuccessResponse(result.data);
      sendJsonResponse(request, doc);
    } else {
      JsonDocument err = result.errorCode.isEmpty()
                             ? svc.buildErrorResponse(result.errorMessage)
                             : svc.buildErrorResponse(result.errorMessage, result.errorCode);
      sendJsonResponse(request, err, 400);
    }
  } else {
    IntegrationService &svc = IntegrationService::shared(_config, _stats, _state);
    JsonDocument err = svc.buildErrorResponse("Service not available", "WEB_SERVICE_UNAVAILABLE");
    sendJsonResponse(request, err, 503);
  }
}

void HAWebServer::setupWebSocket() {
  _webSocket.onEvent(
      [this](AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len) { onWebSocketEvent(server, client, type, arg, data, len); });

  _server.addHandler(&_webSocket);
}

void HAWebServer::setupMDNS() {
  if (MDNS.begin(_config.getDeviceId().c_str())) {
    MDNS.addService("http", "tcp", kServerPort);
    MDNS.addServiceTxt("http", "tcp", "device", "tsuryphone");
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    Logger::infoln(F("mDNS responder started: %s.local"), _config.getDeviceId().c_str());
  } else {
    Logger::errorln(F("Error setting up mDNS responder"));
  }
}

void HAWebServer::handleGetTsuryPhoneConfig(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Get TsuryPhone config requested"));

  JsonDocument commandData;
  executeCommand(request, "tsuryphone_config", commandData.as<JsonVariant>());
}

void HAWebServer::handleRefetchAll(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Refetch all data requested"));

  JsonDocument commandData;
  executeCommand(request, "refetch_all", commandData.as<JsonVariant>());
}

void HAWebServer::handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["number"]) {
    sendErrorResponse(request, "Missing 'number' parameter", 400, "WEB_MISSING_NUMBER");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }

  Logger::infoln(F("HA API: Dial request"));
  executeCommand(request, "dial", json);
}

void HAWebServer::handleDialDigit(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["digit"]) {
    sendErrorResponse(request, "Missing 'digit' parameter", 400, "WEB_MISSING_DIGIT");
    return;
  }

  int digit = -1;
  if (json["digit"].is<int>()) {
    digit = json["digit"].as<int>();
  } else if (json["digit"].is<const char *>()) {
    String digitStr = json["digit"].as<const char *>();
    digitStr.trim();
    if (digitStr.length() == 1 && digitStr[0] >= '0' && digitStr[0] <= '9') {
      digit = digitStr[0] - '0';
    }
  }

  if (digit < 0 || digit > 9) {
    sendErrorResponse(request, "Digit must be between 0 and 9", 400, "WEB_INVALID_DIGIT");
    return;
  }

  Logger::infoln(F("HA API: Dial digit request - %d"), digit);

  JsonDocument commandData;
  commandData["digit"] = digit;
  executeCommand(request, "dial_digit", commandData.as<JsonVariant>());
}

void HAWebServer::handleAnswerCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Answer call request"));

  JsonDocument commandData;
  executeCommand(request, "answer", commandData.as<JsonVariant>());
}

void HAWebServer::handleHangupCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Hangup call request"));

  JsonDocument commandData;
  executeCommand(request, "hangup", commandData.as<JsonVariant>());
}

void HAWebServer::handleSetDND(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  bool hasParam = false;

  JsonObject obj = json.as<JsonObject>();

  // force (optional immediate toggle for DND state)
  if (!obj["force"].isNull()) {
    if (!obj["force"].is<bool>()) {
      sendErrorResponse(
          request, "Invalid 'force' parameter (must be boolean)", 400, "WEB_INVALID_FORCE");
      return;
    }
    hasParam = true;
  }

  // scheduled (optional, primary flag for schedule enablement)
  if (!obj["scheduled"].isNull()) {
    if (!obj["scheduled"].is<bool>()) {
      sendErrorResponse(
          request, "Invalid 'scheduled' parameter (must be boolean)", 400, "WEB_INVALID_SCHEDULED");
      return;
    }
    hasParam = true;
  }

  auto validateIntInRange = [&](const char *key, int minVal, int maxVal, const char *errorCode) {
    JsonVariant v = json[key];
    if (v.isNull()) {
      return false; // not provided
    }
    if (!v.is<int>()) {
      sendErrorResponse(
          request, String("Invalid '") + key + "' parameter (must be integer)", 400, errorCode);
      return true; // indicates we handled (error)
    }
    int iv = v.as<int>();
    if (iv < minVal || iv > maxVal) {
      sendErrorResponse(request,
                        String("' ") + key + "' out of range (" + minVal + "-" + maxVal + ")",
                        400,
                        errorCode);
      return true; // handled (error)
    }
    hasParam = true;
    return false; // no error
  };

  // Optional schedule fields (startHour/endHour 0-23, startMinute/endMinute 0-59)
  if (validateIntInRange("startHour", 0, 23, "WEB_INVALID_START_HOUR")) {
    return;
  }
  if (validateIntInRange("endHour", 0, 23, "WEB_INVALID_END_HOUR")) {
    return;
  }
  if (validateIntInRange("startMinute", 0, 59, "WEB_INVALID_START_MINUTE")) {
    return;
  }
  if (validateIntInRange("endMinute", 0, 59, "WEB_INVALID_END_MINUTE")) {
    return;
  }

  if (!hasParam) {
    sendErrorResponse(request,
                      "At least one DND parameter required (force, scheduled, startHour, endHour, "
                      "startMinute, endMinute)",
                      400,
                      "WEB_DND_PARAM_REQUIRED");
    return;
  }

  Logger::infoln(F("HA API: DND configuration (partial update) request"));
  executeCommand(request, "dnd", json);
}

void HAWebServer::handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["enabled"].is<bool>()) {
    sendErrorResponse(request,
                      "Missing or invalid 'enabled' parameter (must be boolean)",
                      400,
                      "WEB_MISSING_ENABLED");
    return;
  }

  Logger::infoln(F("HA API: Maintenance mode request"));
  executeCommand(request, "maintenance_mode", json);
}

void HAWebServer::handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json) {
  if (json["pattern"]) {
    String p = json["pattern"].as<String>();
    if (!IntegrationValidation::isValidPattern(p)) {
      sendErrorResponse(request, "Invalid ring pattern format", 400, "WEB_INVALID_PATTERN");
      return;
    }
  }

  if (json["force"] && !json["force"].is<bool>()) {
    sendErrorResponse(
        request, "Invalid 'force' parameter (must be boolean)", 400, "WEB_INVALID_FORCE_FLAG");
    return;
  }

  Logger::infoln(F("HA API: Ring operation request"));
  executeCommand(request, "ring", json);
}

void HAWebServer::handleSetVolumeMode(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (json["mode"].isNull() && json["modeCode"].isNull()) {
    sendErrorResponse(
        request, "Missing 'mode' string or 'modeCode' integer", 400, "WEB_INVALID_VOLUME_MODE");
    return;
  }

  Logger::infoln(F("HA API: Volume mode request"));
  executeCommand(request, "volume_mode", json);
}

void HAWebServer::handleResetDevice(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Device reset requested"));

  JsonDocument commandData;
  executeCommand(request, "reset", commandData.as<JsonVariant>());
}

void HAWebServer::handleFactoryReset(AsyncWebServerRequest *request) {
  Logger::warnln(F("HA API: Device factory reset requested"));

  JsonDocument commandData;
  executeCommand(request, "factory_reset", commandData.as<JsonVariant>());
}

void HAWebServer::handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  bool hasValidParam = false;
  if (json["earpieceVolume"].is<int>() || json["earpieceGain"].is<int>() ||
      json["speakerVolume"].is<int>() || json["speakerGain"].is<int>()) {
    hasValidParam = true;
  }

  if (!hasValidParam) {
    sendErrorResponse(request,
                      "At least one audio parameter must be provided: earpieceVolume, "
                      "earpieceGain, speakerVolume, or speakerGain",
                      400,
                      "WEB_AUDIO_PARAM_REQUIRED");
    return;
  }

  Logger::infoln(F("HA API: Audio configuration request"));
  executeCommand(request, "audio_config", json);
}

void HAWebServer::broadcastStateUpdate(const JsonDocument &stateData) {
  String message;
  serializeJson(stateData, message);
  _webSocket.textAll(message);
}

void HAWebServer::setStateUpdateCallback(
    std::function<HAOperationResult(const String &, const JsonVariant &)> callback) {
  _stateUpdateCallback = callback;
}

void HAWebServer::setStatusCallback(std::function<void(JsonObject &)> callback) {
  _statusCallback = callback;
}

void HAWebServer::handleDialQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["code"]) {
    sendErrorResponse(request, "Missing 'code' parameter", 400, "WEB_MISSING_CODE");
    return;
  }
  if (!IntegrationValidation::isValidCode(json["code"].as<String>())) {
    sendErrorResponse(request, "Invalid code format", 400, "WEB_INVALID_CODE");
    return;
  }

  Logger::infoln(F("HA API: Quick dial request"));
  executeCommand(request, "dial_quick_dial", json);
}

void HAWebServer::handleToggleCallWaiting(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Toggle call waiting request"));

  JsonDocument commandData;
  executeCommand(request, "switch_call_waiting", commandData.as<JsonVariant>());
}

void HAWebServer::onWebSocketEvent(AsyncWebSocket *server,
                                   AsyncWebSocketClient *client,
                                   AwsEventType type,
                                   void *arg,
                                   uint8_t *data,
                                   size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    Logger::infoln(F("WebSocket client connected: %u"), client->id());
    break;

  case WS_EVT_DISCONNECT:
    Logger::infoln(F("WebSocket client disconnected: %u"), client->id());
    break;

  case WS_EVT_DATA: {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      String message = (char *)data;
      Logger::infoln(F("WebSocket message received: %s"), message.c_str());

      // Parse and handle WebSocket commands
      JsonDocument doc;
      if (deserializeJson(doc, message) == DeserializationError::Ok) {
        if (_stateUpdateCallback && doc["command"]) {
          HAOperationResult result = _stateUpdateCallback(doc["command"], doc["data"]);
          // WebSocket commands don't return responses to the client currently
          // But we could add that functionality here if needed
        }
      }
    }
    break;
  }

  case WS_EVT_PING:
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void HAWebServer::sendJsonResponse(AsyncWebServerRequest *request,
                                   const JsonDocument &doc,
                                   int statusCode) {
  String response;
  serializeJson(doc, response);
  request->send(statusCode, "application/json", response);
}

void HAWebServer::sendErrorResponse(AsyncWebServerRequest *request,
                                    const String &error,
                                    int statusCode,
                                    const String &errorCode) {
  IntegrationService &svc = IntegrationService::shared(_config, _stats, _state);
  JsonDocument err = errorCode.isEmpty() ? svc.buildErrorResponse(error)
                                         : svc.buildErrorResponse(error, errorCode);
  sendJsonResponse(request, err, statusCode);
}

void HAWebServer::handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["code"] || !json["number"]) {
    sendErrorResponse(
        request, "Missing required parameters: 'code' and 'number'", 400, "WEB_MISSING_CODE");
    return;
  }
  if (!IntegrationValidation::isValidCode(json["code"].as<String>())) {
    sendErrorResponse(request, "Invalid code format", 400, "WEB_INVALID_CODE");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }

  Logger::infoln(F("HA API: Add quick dial request"));
  executeCommand(request, "quick_dial_add", json);
}

void HAWebServer::handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }
  if (!json["code"]) {
    sendErrorResponse(request, "Missing required parameter: 'code'", 400, "WEB_MISSING_CODE");
    return;
  }
  if (!IntegrationValidation::isValidCode(json["code"].as<String>())) {
    sendErrorResponse(request, "Invalid code format", 400, "WEB_INVALID_CODE");
    return;
  }

  Logger::infoln(F("HA API: Remove quick dial request"));
  executeCommand(request, "quick_dial_remove", json);
}

void HAWebServer::handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["code"] || !json["id"]) {
    sendErrorResponse(
        request, "Missing required parameters: 'code' and 'id'", 400, "WEB_MISSING_CODE");
    return;
  }
  if (!IntegrationValidation::isValidCode(json["code"].as<String>())) {
    sendErrorResponse(request, "Invalid code format", 400, "WEB_INVALID_CODE");
    return;
  }
  if (!IntegrationValidation::isValidWebhookId(json["id"].as<String>())) {
    sendErrorResponse(request, "Invalid id format", 400, "WEB_INVALID_ID");
    return;
  }

  Logger::infoln(F("HA API: Add webhook action request"));
  executeCommand(request, "webhook_add", json);
}

void HAWebServer::handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["code"]) {
    sendErrorResponse(request, "Missing required parameter: 'code'", 400, "WEB_MISSING_CODE");
    return;
  }
  if (!IntegrationValidation::isValidCode(json["code"].as<String>())) {
    sendErrorResponse(request, "Invalid code format", 400, "WEB_INVALID_CODE");
    return;
  }

  Logger::infoln(F("HA API: Remove webhook action request"));
  executeCommand(request, "webhook_remove", json);
}

void HAWebServer::handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["number"]) {
    sendErrorResponse(request, "Missing required parameter: 'number'", 400, "WEB_MISSING_NUMBER");
    return;
  }
  if (!json["name"]) {
    sendErrorResponse(request, "Missing required parameter: 'name'", 400, "WEB_MISSING_NAME");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }

  String name = json["name"].as<String>();
  name.trim();
  if (name.isEmpty()) {
    sendErrorResponse(request, "name cannot be empty", 400, "WEB_INVALID_NAME");
    return;
  }
  json["name"] = name;

  Logger::infoln(F("HA API: Add blocked number request"));
  executeCommand(request, "blocked_add", json);
}

void HAWebServer::handleAddPriorityCaller(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }
  if (!json["number"]) {
    sendErrorResponse(request, "Missing required parameter: 'number'", 400, "WEB_MISSING_NUMBER");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }
  Logger::infoln(F("HA API: Add priority caller request"));
  executeCommand(request, "priority_add", json);
}

void HAWebServer::handleRemovePriorityCaller(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }
  if (!json["number"]) {
    sendErrorResponse(request, "Missing required parameter: 'number'", 400, "WEB_MISSING_NUMBER");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }
  Logger::infoln(F("HA API: Remove priority caller request"));
  executeCommand(request, "priority_remove", json);
}

void HAWebServer::handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["number"]) {
    sendErrorResponse(request, "Missing required parameter: 'number'", 400, "WEB_MISSING_NUMBER");
    return;
  }
  if (!IntegrationValidation::isValidNumber(json["number"].as<String>())) {
    sendErrorResponse(request, "Invalid number format", 400, "WEB_INVALID_NUMBER");
    return;
  }

  Logger::infoln(F("HA API: Remove blocked number request"));
  executeCommand(request, "blocked_remove", json);
}

void HAWebServer::handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["pattern"]) {
    sendErrorResponse(request, "Missing required parameter: 'pattern'", 400, "WEB_INVALID_PATTERN");
    return;
  }
  if (!IntegrationValidation::isValidPattern(json["pattern"].as<String>())) {
    sendErrorResponse(request, "Invalid ring pattern format", 400, "WEB_INVALID_PATTERN");
    return;
  }

  Logger::infoln(F("HA API: Set ring pattern request"));
  executeCommand(request, "ring_pattern", json);
}

void HAWebServer::handleSetDialingConfig(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  JsonObject obj = json.as<JsonObject>();
  JsonVariant defaultCodeVariant = obj["defaultCode"];

  if (defaultCodeVariant.isNull()) {
    sendErrorResponse(
        request, "Missing required parameter: 'defaultCode'", 400, "WEB_MISSING_DEFAULT_CODE");
    return;
  }

  if (!(defaultCodeVariant.is<const char *>() || defaultCodeVariant.is<String>())) {
    sendErrorResponse(
        request, "Invalid defaultCode value (must be a string)", 400, "WEB_INVALID_DEFAULT_CODE");
    return;
  }

  String requestedCode = defaultCodeVariant.as<String>();
  requestedCode.trim();
  if (requestedCode.isEmpty()) {
    sendErrorResponse(request,
                      "Invalid defaultCode value (must contain digits)",
                      400,
                      "WEB_INVALID_DEFAULT_CODE");
    return;
  }

  String sanitized = PhoneNormalization::sanitizeDefaultDialingCode(requestedCode);
  if (sanitized.isEmpty()) {
    sendErrorResponse(request,
                      "Invalid defaultCode value (must contain digits)",
                      400,
                      "WEB_INVALID_DEFAULT_CODE");
    return;
  }

  obj["defaultCode"] = sanitized;

  Logger::infoln(F("HA API: Set dialing configuration request (defaultCode=%s)"),
                 sanitized.c_str());
  executeCommand(request, "dialing_config", json);
}

void HAWebServer::handleSetHAUrl(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON object", 400, "WEB_INVALID_JSON");
    return;
  }

  if (!json["url"]) {
    sendErrorResponse(request, "Missing required parameter: 'url'", 400, "WEB_INVALID_URL");
    return;
  }
  if (!IntegrationValidation::isValidUrl(json["url"].as<String>())) {
    sendErrorResponse(request, "Invalid URL format", 400, "WEB_INVALID_URL");
    return;
  }

  Logger::infoln(F("HA API: Set HA URL request"));
  executeCommand(request, "ha_url", json);
}

void HAWebServer::handleDiagnostics(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Diagnostics requested"));
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  // Generic baseline
  populateCoreDiagnostics(root, _config, _stats);

  // HA-specific capability decorations (kept minimal & additive)
  JsonArray caps = root["capabilities"].to<JsonArray>();
  caps.add("actions");
  caps.add("number_code_handler");

  // Action codes if obtainable via command interface (reuse existing command path):
  // We trigger a synthetic command 'tsuryphone_config' then mine action codes if present.
  // For now, we just mark presence; full enumeration lives in structured log on startup.
  root["actionCodesExposedInLogs"] = true;

  sendJsonResponse(request, doc);
}

#endif
