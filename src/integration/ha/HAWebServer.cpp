#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAWebServer.h"
#include "../../common/logger.h"
#include "../../common/state.h"
#include "../../common/timeManager.h"
#include "../../config.h"
#include "../../core/DeviceConfig.h"
#include "../../core/DeviceStats.h"
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

  _server.begin();

  // Setup mDNS for auto-discovery
  if (MDNS.begin(_config.getDeviceId().c_str())) {
    MDNS.addService("http", "tcp", kServerPort);
    MDNS.addServiceTxt("http", "tcp", "device", "tsuryphone");
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    Logger::infoln(F("mDNS responder started: %s.local"), _config.getDeviceId().c_str());
  } else {
    Logger::errorln(F("Error setting up mDNS responder"));
  }

  Logger::infoln(F("HA Web Server started successfully"));
  return true;
}

void HAWebServer::process() {
  // Only cleanup WebSocket clients every 60 seconds to reduce overhead
  unsigned long now = millis();
  if (now - _lastCleanupTime >= kWebSocketCleanupInterval) {
    _webSocket.cleanupClients();
    _lastCleanupTime = now;
  }
}

void HAWebServer::stop() {
  // Close all WebSocket connections gracefully
  _webSocket.closeAll();
  
  // Stop the web server
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
  _server.on("/api/config/tsuryphone", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleGetTsuryPhoneConfig(request);
  });

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

void HAWebServer::handleGetTsuryPhoneConfig(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["success"] = true;
  JsonObject data = doc["data"].to<JsonObject>();
  data["deviceId"] = _config.getDeviceId();
  sendJsonResponse(request, doc);
}

void HAWebServer::handleRefetchAll(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Refetch all data requested"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    HAOperationResult result = _stateUpdateCallback("refetch_all", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      
      // If the integration returned data, use it; otherwise build our own
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      } else {
        JsonObject data = doc["data"].to<JsonObject>();
        // Add all hierarchical data using helper functions
        addStatus(data);
        addConfig(data);
        addStats(data);
        addPhone(data);
      }
      
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    // Fallback to original behavior if callback not available
    _config.load();
    _stats.load();

    JsonDocument doc;
    doc["success"] = true;

    JsonObject data = doc["data"].to<JsonObject>();

    // Add all hierarchical data using helper functions
    addStatus(data);
    addConfig(data);
    addStats(data);
    addPhone(data);

    sendJsonResponse(request, doc);
  }
}

void HAWebServer::handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  if (!json["number"]) {
    sendErrorResponse(request, "Missing 'number' parameter");
    return;
  }

  String number = json["number"].as<String>();
  Logger::infoln(F("HA API: Dial request for %s"), number.c_str());

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    commandData["number"] = number;
    HAOperationResult result = _stateUpdateCallback("dial", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleAnswerCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Answer call request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    HAOperationResult result = _stateUpdateCallback("answer", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleHangupCall(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Hangup call request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    HAOperationResult result = _stateUpdateCallback("hangup", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleSetDND(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: DND configuration request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("dnd", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Maintenance mode request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("maintenance_mode", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Ring operation request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("ring", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleResetDevice(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Device reset requested"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    HAOperationResult result = _stateUpdateCallback("reset", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Audio configuration request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("audio_config", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
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
  Logger::infoln(F("HA API: Quick dial request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("dial_quick_dial", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleToggleCallWaiting(AsyncWebServerRequest *request) {
  Logger::infoln(F("HA API: Toggle call waiting request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    JsonDocument commandData;
    HAOperationResult result = _stateUpdateCallback("switch_call_waiting", commandData.as<JsonVariant>());
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
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
                                    int statusCode) {
  JsonDocument doc;
  doc["success"] = false;
  doc["message"] = error;
  sendJsonResponse(request, doc, statusCode);
}

void HAWebServer::handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Add quick dial request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("quick_dial_add", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Remove quick dial request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("quick_dial_remove", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Add webhook action request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("webhook_add", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Remove webhook action request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("webhook_remove", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Add blocked number request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("blocked_number_add", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Remove blocked number request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("blocked_number_remove", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Set ring pattern request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("ring_pattern", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

void HAWebServer::handleSetHAUrl(AsyncWebServerRequest *request, JsonVariant &json) {
  Logger::infoln(F("HA API: Set HA URL request"));

  // Delegate to integration business logic
  if (_stateUpdateCallback) {
    HAOperationResult result = _stateUpdateCallback("ha_url", json);
    
    if (result.success) {
      JsonDocument doc;
      doc["success"] = true;
      if (!result.data.isNull()) {
        doc["data"] = result.data;
      }
      sendJsonResponse(request, doc);
    } else {
      sendErrorResponse(request, result.errorMessage);
    }
  } else {
    sendErrorResponse(request, "Service not available");
  }
}

// Helper functions for building structured data responses
void HAWebServer::addStatus(JsonObject &doc) {
  JsonObject status = doc["status"].to<JsonObject>();

  // System status
  JsonObject systemStatus = status["system"].to<JsonObject>();
  systemStatus["uptime"] = _stats.getUptime();
  systemStatus["freeHeap"] = _stats.getFreeHeap();
  systemStatus["rssi"] = _stats.getRSSI();

  // Phone status
  JsonObject phoneStatus = status["phone"].to<JsonObject>();
  phoneStatus["maintenanceMode"] = _state.isMaintenanceMode;
  phoneStatus["dndActive"] = _state.isDnd;

  // Get state from callback if available
  if (_statusCallback) {
    JsonDocument tempDoc;
    JsonObject tempObj = tempDoc.to<JsonObject>();
    _statusCallback(tempObj);
    if (tempObj["state"]) {
      phoneStatus["state"] = tempObj["state"];
    }
  } else {
    phoneStatus["state"] = "unknown";
  }
}

void HAWebServer::addConfig(JsonObject &doc) {
  JsonObject config = doc["config"].to<JsonObject>();

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
  dnd["schedule"] = dndConfig.scheduled;
  dnd["startMinute"] = dndConfig.startMinute;
  dnd["endMinute"] = dndConfig.endMinute;
  dnd["startHour"] = dndConfig.startHour;
  dnd["endHour"] = dndConfig.endHour;

  // Phone config
  JsonObject phone = config["phone"].to<JsonObject>();
  phone["ringPattern"] = _config.getRingPattern();
}

void HAWebServer::addStats(JsonObject &doc) {
  const CallStats &callStats = _stats.getCallStats();
  JsonObject stats = doc["stats"].to<JsonObject>();

  JsonObject calls = stats["calls"].to<JsonObject>();
  JsonObject totals = calls["totals"].to<JsonObject>();
  totals["calls"] = callStats.totalCalls;
  totals["incoming"] = callStats.incomingCalls;
  totals["outgoing"] = callStats.outgoingCalls;
  totals["blocked"] = callStats.blockedCalls;
  totals["talkTime"] = callStats.totalTalkTimeSeconds;

  JsonObject lastCall = calls["lastCall"].to<JsonObject>();
  lastCall["number"] = callStats.lastCall.number;
  lastCall["type"] = callStats.lastCall.type;

  JsonObject systemStats = stats["system"].to<JsonObject>();
  systemStats["resets"] = _stats.getResetCount();
}

void HAWebServer::addPhone(JsonObject &doc) {
  JsonObject phone = doc["phone"].to<JsonObject>();

  // Quick dial entries
  JsonArray quickDial = phone["quickDial"].to<JsonArray>();
  for (const auto &entry : _config.getQuickDialEntries()) {
    JsonObject entryObj = quickDial.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["number"] = entry.number;
    entryObj["name"] = entry.name;
  }

  // Blocked numbers
  JsonArray blocked = phone["blocked"].to<JsonArray>();
  for (const auto &entry : _config.getBlockedNumbers()) {
    JsonObject entryObj = blocked.add<JsonObject>();
    entryObj["number"] = entry.number;
    entryObj["reason"] = entry.reason;
  }

  // Webhook actions
  JsonArray webhooks = phone["webhooks"].to<JsonArray>();
  for (const auto &entry : _config.getWebhookActions()) {
    JsonObject entryObj = webhooks.add<JsonObject>();
    entryObj["code"] = entry.code;
    entryObj["id"] = entry.id;
    entryObj["actionName"] = entry.actionName;
  }
}

#endif // HOME_ASSISTANT_INTEGRATION
