#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAWebServer.h"
#include "../../common/logger.h"
#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "../../core/DeviceStats.h"
#include <ESPmDNS.h>

const char *HAWebServer::kWebSocketPath = "/ws";

HAWebServer::HAWebServer(DeviceConfig &config, DeviceStats &stats)
    : _config(config), _stats(stats), _server(kServerPort), _webSocket(kWebSocketPath) {}

bool HAWebServer::init() {
  Logger::infoln(F("Initializing HA Web Server on port %d..."), kServerPort);

  setupRoutes();
  setupWebSocket();

  _server.begin();

  // Setup mDNS for auto-discovery
  if (MDNS.begin(_config.getDeviceName().c_str())) {
    MDNS.addService("http", "tcp", kServerPort);
    MDNS.addServiceTxt("http", "tcp", "device", "tsuryphone");
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    Logger::infoln(F("mDNS responder started: %s.local"), _config.getDeviceName().c_str());
  } else {
    Logger::errorln(F("Error setting up mDNS responder"));
  }

  Logger::infoln(F("HA Web Server started successfully"));
  return true;
}

void HAWebServer::process() {
  _webSocket.cleanupClients();
}

void HAWebServer::stop() {
  _server.end();
  MDNS.end();
}

void HAWebServer::setupRoutes() {
  // Enable CORS for all routes
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // Handle preflight OPTIONS requests
  _server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  // Data endpoints
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetStatus(request);
  });

  _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetConfig(request);
  });

  _server.on(
      "/api/stats", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetStats(request); });

  _server.on("/api/refetch_all", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRefetchAll(request);
  });

  // Call control endpoints
  _server.on(
      "/api/call/dial",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // POST handler will be called after body parsing
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleDialNumber(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on("/api/call/answer", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleAnswerCall(request);
  });

  _server.on("/api/call/hangup", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleHangupCall(request);
  });

  _server.on("/api/call/switch_call_waiting", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleToggleCallWaiting(request);
  });

  _server.on(
      "/api/call/dial_quick_dial",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleDialQuickDial(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  // Configuration endpoints
  _server.on(
      "/api/config/dnd",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleSetDND(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/maintenance",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleSetMaintenanceMode(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/audio",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleSetAudioConfig(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/ring_pattern",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleSetRingPattern(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/quick_dial_add",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleAddQuickDial(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/quick_dial_remove",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleRemoveQuickDial(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/webhook_add",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleAddWebhookAction(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/webhook_remove",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleRemoveWebhookAction(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/blocked_number_add",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleAddBlockedNumber(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on(
      "/api/config/blocked_number_remove",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleRemoveBlockedNumber(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  // System control endpoints
  _server.on(
      "/api/system/ring",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleRingOperation(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });

  _server.on("/api/system/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleResetDevice(request);
  });

  _server.on(
      "/api/config/ha_url",
      HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Will be handled in body callback
      },
      NULL,
      [this](
          AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
          JsonVariant variant = doc.as<JsonVariant>();
          handleSetHAUrl(request, variant);
        } else {
          sendErrorResponse(request, "Invalid JSON");
        }
      });
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

void HAWebServer::handleGetStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();

  // Use status callback to get comprehensive status information
  if (_statusCallback) {
    _statusCallback(obj);
  } else {
    // Fallback to basic device info if no callback is set
    obj["deviceName"] = _config.getDeviceName();
    obj["deviceId"] = _config.getDeviceId();
    obj["uptime"] = _stats.getUptime();
    obj["freeHeap"] = _stats.getFreeHeap();
    obj["rssi"] = _stats.getRSSI();
    obj["maintenanceMode"] = _config.isMaintenanceMode();
    obj["state"] = "unknown";
  }

  sendJsonResponse(request, doc);
}

void HAWebServer::handleGetConfig(AsyncWebServerRequest *request) {
  JsonDocument doc;

  // Device info
  JsonObject device = doc["device"].to<JsonObject>();
  device["name"] = _config.getDeviceName();
  device["id"] = _config.getDeviceId();

  // Audio config
  const AudioConfig &audioConfig = _config.getAudioConfig();
  JsonObject audio = doc["audio"].to<JsonObject>();
  audio["earpieceVolume"] = audioConfig.earpieceVolume;
  audio["earpieceGain"] = audioConfig.earpieceGain;
  audio["speakerVolume"] = audioConfig.speakerVolume;
  audio["speakerGain"] = audioConfig.speakerGain;

  // DND config
  const DndConfig &dndConfig = _config.getDndConfig();
  JsonObject dnd = doc["dnd"].to<JsonObject>();
  dnd["force"] = dndConfig.force;
  dnd["scheduled"] = dndConfig.scheduled;
  dnd["startHour"] = dndConfig.startHour;
  dnd["startMinute"] = dndConfig.startMinute;
  dnd["endHour"] = dndConfig.endHour;
  dnd["endMinute"] = dndConfig.endMinute;

  // Quick dial entries
  JsonObject quickDial = doc["quickDial"].to<JsonObject>();
  for (const auto &entry : _config.getQuickDialEntries()) {
    quickDial[entry.first] = entry.second;
  }

  // Blocked numbers
  JsonArray blocked = doc["blockedNumbers"].to<JsonArray>();
  for (const String &number : _config.getBlockedNumbers()) {
    blocked.add(number);
  }

  // Webhook actions
  JsonObject webhooks = doc["webhookActions"].to<JsonObject>();
  for (const auto &action : _config.getWebhookActions()) {
    webhooks[action.first] = action.second;
  }

  // Ring pattern
  doc["ringPattern"] = _config.getRingPattern();

  sendJsonResponse(request, doc);
}

void HAWebServer::handleRefetchAll(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Refetch all data requested"));

  // Reload configuration from SPIFFS
  _config.load();
  _stats.load();

  JsonDocument doc;

  // Get status data
  JsonObject status = doc["status"].to<JsonObject>();
  if (_statusCallback) {
    _statusCallback(status);
  } else {
    // Fallback to basic device info if no callback is set
    status["deviceName"] = _config.getDeviceName();
    status["deviceId"] = _config.getDeviceId();
    status["uptime"] = _stats.getUptime();
    status["freeHeap"] = _stats.getFreeHeap();
    status["rssi"] = _stats.getRSSI();
    status["maintenanceMode"] = _config.isMaintenanceMode();
    status["state"] = "unknown";
  }

  // Get stats data
  JsonObject stats = doc["stats"].to<JsonObject>();
  const CallStats &callStats = _stats.getCallStats();
  stats["totalCalls"] = callStats.totalCalls;
  stats["incomingCalls"] = callStats.incomingCalls;
  stats["outgoingCalls"] = callStats.outgoingCalls;
  stats["blockedCalls"] = callStats.blockedCalls;
  stats["totalTalkTimeSeconds"] = callStats.totalTalkTimeSeconds;
  stats["lastCall"] = callStats.lastCall;
  stats["resetCount"] = _config.getResetCount();
  stats["uptime"] = _stats.getUptime();
  stats["freeHeap"] = _stats.getFreeHeap();
  stats["rssi"] = _stats.getRSSI();

  // Get config data
  JsonObject config = doc["config"].to<JsonObject>();
  // Device info
  JsonObject device = config["device"].to<JsonObject>();
  device["name"] = _config.getDeviceName();
  device["id"] = _config.getDeviceId();

  // Audio config
  const AudioConfig &audioConfig = _config.getAudioConfig();
  JsonObject audio = config["audio"].to<JsonObject>();
  audio["earpieceVolume"] = audioConfig.earpieceVolume;
  audio["earpieceGain"] = audioConfig.earpieceGain;
  audio["speakerVolume"] = audioConfig.speakerVolume;
  audio["speakerGain"] = audioConfig.speakerGain;

  // DND config
  const DndConfig &dndConfig = _config.getDndConfig();
  JsonObject dnd = config["dnd"].to<JsonObject>();
  dnd["force"] = dndConfig.force;
  dnd["scheduled"] = dndConfig.scheduled;
  dnd["startHour"] = dndConfig.startHour;
  dnd["startMinute"] = dndConfig.startMinute;
  dnd["endHour"] = dndConfig.endHour;
  dnd["endMinute"] = dndConfig.endMinute;

  // Quick dial entries
  JsonObject quickDial = config["quickDial"].to<JsonObject>();
  for (const auto &entry : _config.getQuickDialEntries()) {
    quickDial[entry.first] = entry.second;
  }

  // Blocked numbers
  JsonArray blocked = config["blockedNumbers"].to<JsonArray>();
  for (const String &number : _config.getBlockedNumbers()) {
    blocked.add(number);
  }

  // Webhook actions
  JsonObject webhooks = config["webhookActions"].to<JsonObject>();
  for (const auto &action : _config.getWebhookActions()) {
    webhooks[action.first] = action.second;
  }

  // Ring pattern
  config["ringPattern"] = _config.getRingPattern();

  sendJsonResponse(request, doc);
}

void HAWebServer::handleGetStats(AsyncWebServerRequest *request) {
  JsonDocument doc;

  const CallStats &callStats = _stats.getCallStats();

  doc["totalCalls"] = callStats.totalCalls;
  doc["incomingCalls"] = callStats.incomingCalls;
  doc["outgoingCalls"] = callStats.outgoingCalls;
  doc["blockedCalls"] = callStats.blockedCalls;
  doc["totalTalkTimeSeconds"] = callStats.totalTalkTimeSeconds;
  doc["lastCall"] = callStats.lastCall;
  doc["resetCount"] = _config.getResetCount();
  doc["uptime"] = _stats.getUptime();
  doc["freeHeap"] = _stats.getFreeHeap();
  doc["rssi"] = _stats.getRSSI();

  sendJsonResponse(request, doc);
}

void HAWebServer::handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["number"]) {
    sendErrorResponse(request, "Missing 'number' parameter");
    return;
  }

  String number = json["number"].as<String>();
  Logger::infoln(F("HA API: Dial request for %s"), number.c_str());

  // Send dial command to integration
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    commandData["number"] = number;
    _stateUpdateCallback("dial", commandData.as<JsonVariant>());
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Dial request queued";
  doc["number"] = number;
  sendJsonResponse(request, doc);
}

void HAWebServer::handleAnswerCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Answer call request"));

  // Send answer command to integration
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    _stateUpdateCallback("answer", commandData.as<JsonVariant>());
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Call answered";
  sendJsonResponse(request, doc);
}

void HAWebServer::handleHangupCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Hangup call request"));

  // Send hangup command to integration
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    _stateUpdateCallback("hangup", commandData.as<JsonVariant>());
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Call hung up";
  sendJsonResponse(request, doc);
}

void HAWebServer::handleSetDND(AsyncWebServerRequest *request, JsonVariant &json) {
  DndConfig dndConfig = _config.getDndConfig();
  bool changed = false;

  if (json["force"].is<bool>()) {
    dndConfig.force = json["force"];
    changed = true;
  }

  if (json["scheduled"].is<bool>()) {
    dndConfig.scheduled = json["scheduled"];
    changed = true;
  }

  if (json["startHour"].is<int>()) {
    dndConfig.startHour = json["startHour"];
    changed = true;
  }

  if (json["startMinute"].is<int>()) {
    dndConfig.startMinute = json["startMinute"];
    changed = true;
  }

  if (json["endHour"].is<int>()) {
    dndConfig.endHour = json["endHour"];
    changed = true;
  }

  if (json["endMinute"].is<int>()) {
    dndConfig.endMinute = json["endMinute"];
    changed = true;
  }

  if (changed) {
    _config.setDndConfig(dndConfig);
    Logger::infoln(F("HA API: DND configuration updated"));
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "DND configuration updated";
  sendJsonResponse(request, doc);
}

void HAWebServer::handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["enabled"].is<bool>()) {
    sendErrorResponse(request, "Missing 'enabled' parameter");
    return;
  }

  bool enabled = json["enabled"];
  _config.setMaintenanceMode(enabled);

  Logger::infoln(F("HA API: Maintenance mode %s"), enabled ? F("enabled") : F("disabled"));

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = enabled ? "Maintenance mode enabled" : "Maintenance mode disabled";
  doc["maintenanceMode"] = enabled;
  sendJsonResponse(request, doc);
}

void HAWebServer::handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["pattern"]) {
    sendErrorResponse(request, "Missing 'pattern' parameter");
    return;
  }

  String pattern = json["pattern"].as<String>();
  Logger::infoln(F("HA API: Ring operation with pattern: %s"), pattern.c_str());

  // Send ring command to integration
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    commandData["pattern"] = pattern;
    _stateUpdateCallback("ring", commandData.as<JsonVariant>());
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Ring operation queued";
  doc["pattern"] = pattern;
  sendJsonResponse(request, doc);
}

void HAWebServer::handleResetDevice(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Device reset requested"));

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Device reset initiated";
  sendJsonResponse(request, doc);

  // Send graceful shutdown notification via WebSocket
  JsonDocument shutdownDoc;
  JsonObject shutdownObj = shutdownDoc.to<JsonObject>();
  shutdownObj["event"] = "system";
  shutdownObj["type"] = "shutdown";
  shutdownObj["timestamp"] = millis();
  shutdownObj["reason"] = "reset_requested";
  shutdownObj["message"] = "Device is shutting down for reset";

  broadcastStateUpdate(shutdownDoc);

  // Give time for WebSocket message to be sent and connections to close gracefully
  delay(2000);

  // Close all WebSocket connections gracefully
  _webSocket.closeAll();

  // Stop the web server
  _server.end();

  // Additional delay to ensure cleanup
  delay(500);

  ESP.restart();
}

void HAWebServer::handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json) {
  AudioConfig audioConfig = _config.getAudioConfig();
  bool changed = false;

  if (json["earpieceVolume"].is<int>()) {
    int volume = json["earpieceVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.earpieceVolume = volume;
      changed = true;
    } else {
      JsonDocument doc;
      doc["status"] = "error";
      doc["message"] = "Earpiece volume must be between 1 and 7";
      sendJsonResponse(request, doc);
      return;
    }
  }

  if (json["earpieceGain"].is<int>()) {
    int gain = json["earpieceGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.earpieceGain = gain;
      changed = true;
    } else {
      JsonDocument doc;
      doc["status"] = "error";
      doc["message"] = "Earpiece gain must be between 1 and 7";
      sendJsonResponse(request, doc);
      return;
    }
  }

  if (json["speakerVolume"].is<int>()) {
    int volume = json["speakerVolume"];
    if (volume >= 1 && volume <= 7) {
      audioConfig.speakerVolume = volume;
      changed = true;
    } else {
      JsonDocument doc;
      doc["status"] = "error";
      doc["message"] = "Speaker volume must be between 1 and 7";
      sendJsonResponse(request, doc);
      return;
    }
  }

  if (json["speakerGain"].is<int>()) {
    int gain = json["speakerGain"];
    if (gain >= 1 && gain <= 7) {
      audioConfig.speakerGain = gain;
      changed = true;
    } else {
      JsonDocument doc;
      doc["status"] = "error";
      doc["message"] = "Speaker gain must be between 1 and 7";
      sendJsonResponse(request, doc);
      return;
    }
  }

  if (changed) {
    _config.setAudioConfig(audioConfig);
    Logger::infoln(F("HA API: Audio configuration updated"));
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Audio configuration updated";
  sendJsonResponse(request, doc);
}

void HAWebServer::broadcastStateUpdate(const JsonDocument &stateData) {
  String message;
  serializeJson(stateData, message);
  _webSocket.textAll(message);
}

void HAWebServer::setStateUpdateCallback(
    std::function<void(const String &, const JsonVariant &)> callback) {
  _stateUpdateCallback = callback;
}

void HAWebServer::setStatusCallback(std::function<void(JsonObject &)> callback) {
  _statusCallback = callback;
}

void HAWebServer::handleDialQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["code"]) {
    sendErrorResponse(request, "Missing 'code' parameter");
    return;
  }

  // Convert to string regardless of whether it's a string or number in JSON
  String code = json["code"].as<String>();

  // Look up the quick dial entry
  auto quickDialEntries = _config.getQuickDialEntries();
  auto it = quickDialEntries.find(code);

  if (it == quickDialEntries.end()) {
    sendErrorResponse(request, "Quick dial code not found");
    return;
  }

  String number = it->second;

  // Trigger the actual dial operation
  if (_stateUpdateCallback) {
    JsonDocument callbackDoc;
    callbackDoc["number"] = number;
    _stateUpdateCallback("dial", callbackDoc.as<JsonVariant>());
  }

  Logger::infoln(F("HA API: Quick dial %s -> %s"), code.c_str(), number.c_str());

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = String("Dialing quick dial entry ") + code + " -> " + number;
  sendJsonResponse(request, doc);
}

void HAWebServer::handleToggleCallWaiting(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Toggle call waiting request"));

  // Send call waiting toggle command to integration
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    _stateUpdateCallback("switch_call_waiting", commandData.as<JsonVariant>());
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Call waiting toggled";
  sendJsonResponse(request, doc);
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
          _stateUpdateCallback(doc["command"], doc["data"]);
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
                                    int statusCode) {
  JsonDocument doc;
  doc["status"] = "error";
  doc["message"] = error;
  sendJsonResponse(request, doc, statusCode);
}

void HAWebServer::handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"] || !jsonObj["number"]) {
    sendErrorResponse(request, "Missing required parameters: code, number", 400);
    return;
  }

  // Convert to string regardless of whether they're strings or numbers in JSON
  String code = jsonObj["code"].as<String>();
  String number = jsonObj["number"].as<String>();

  if (code.isEmpty() || number.isEmpty()) {
    sendErrorResponse(request, "Code and number cannot be empty", 400);
    return;
  }

  if (_config.hasQuickDialEntry(code) || _config.hasWebhookAction(code)) {
    sendErrorResponse(request, "Code already exists in quick dial or webhook actions", 400);
    return;
  }

  if (_config.addQuickDialEntry(code, number)) {
    doc["success"] = true;
    doc["message"] = "Quick dial entry added successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Failed to add quick dial entry", 500);
  }
}

void HAWebServer::handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"]) {
    sendErrorResponse(request, "Missing required parameter: code", 400);
    return;
  }

  // Convert to string regardless of whether it's a string or number in JSON
  String code = jsonObj["code"].as<String>();

  if (code.isEmpty()) {
    sendErrorResponse(request, "Code cannot be empty", 400);
    return;
  }

  if (_config.removeQuickDialEntry(code)) {
    doc["success"] = true;
    doc["message"] = "Quick dial entry removed successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Quick dial entry not found", 404);
  }
}

void HAWebServer::handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"] || !jsonObj["id"]) {
    sendErrorResponse(request, "Missing required parameters: code, id", 400);
    return;
  }

  // Convert to string regardless of whether they're strings or numbers in JSON
  String code = jsonObj["code"].as<String>();
  String webhookId = jsonObj["id"].as<String>();

  if (code.isEmpty() || webhookId.isEmpty()) {
    sendErrorResponse(request, "Code and webhook ID cannot be empty", 400);
    return;
  }

  if (_config.hasQuickDialEntry(code) || _config.hasWebhookAction(code)) {
    sendErrorResponse(request, "Code already exists in quick dial or webhook actions", 400);
    return;
  }

  if (_config.addWebhookAction(code, webhookId)) {
    doc["success"] = true;
    doc["message"] = "Webhook action added successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Failed to add webhook action", 500);
  }
}

void HAWebServer::handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["code"]) {
    sendErrorResponse(request, "Missing required parameter: code", 400);
    return;
  }

  // Convert to string regardless of whether it's a string or number in JSON
  String code = jsonObj["code"].as<String>();

  if (code.isEmpty()) {
    sendErrorResponse(request, "Code cannot be empty", 400);
    return;
  }

  if (_config.removeWebhookAction(code)) {
    doc["success"] = true;
    doc["message"] = "Webhook action removed successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Webhook action not found", 404);
  }
}

void HAWebServer::handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["number"]) {
    sendErrorResponse(request, "Missing required parameter: number", 400);
    return;
  }

  // Convert to string regardless of whether it's a string or number in JSON
  String number = jsonObj["number"].as<String>();

  if (number.isEmpty()) {
    sendErrorResponse(request, "Number cannot be empty", 400);
    return;
  }

  if (_config.addBlockedNumber(number)) {
    doc["success"] = true;
    doc["message"] = "Blocked number added successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Failed to add blocked number (may already exist)", 400);
  }
}

void HAWebServer::handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["number"]) {
    sendErrorResponse(request, "Missing required parameter: number", 400);
    return;
  }

  // Convert to string regardless of whether it's a string or number in JSON
  String number = jsonObj["number"].as<String>();

  if (number.isEmpty()) {
    sendErrorResponse(request, "Number cannot be empty", 400);
    return;
  }

  if (_config.removeBlockedNumber(number)) {
    doc["success"] = true;
    doc["message"] = "Blocked number removed successfully";
    sendJsonResponse(request, doc);
  } else {
    sendErrorResponse(request, "Blocked number not found", 404);
  }
}

void HAWebServer::handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json) {
  String pattern = json["pattern"].as<String>();

  if (pattern.isEmpty()) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = "Ring pattern cannot be empty";
    sendJsonResponse(request, doc);
    return;
  }

  _config.setRingPattern(pattern);
  Logger::infoln(F("HA API: Ring pattern set to: %s"), pattern.c_str());

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Ring pattern updated successfully";
  doc["pattern"] = pattern;
  sendJsonResponse(request, doc);
}

void HAWebServer::handleSetHAUrl(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonDocument doc;

  if (!json.is<JsonObject>()) {
    sendErrorResponse(request, "Invalid JSON", 400);
    return;
  }

  JsonObject jsonObj = json.as<JsonObject>();

  if (!jsonObj["url"]) {
    sendErrorResponse(request, "Missing required parameter: url", 400);
    return;
  }

  String url = jsonObj["url"].as<String>();

  if (url.isEmpty()) {
    sendErrorResponse(request, "URL cannot be empty", 400);
    return;
  }

  // Set the HA URL in device config for persistence
  _config.setHomeAssistantUrl(url);
  doc["success"] = true;
  doc["message"] = "Home Assistant URL set successfully";
  sendJsonResponse(request, doc);
}

#endif // HOME_ASSISTANT_INTEGRATION
