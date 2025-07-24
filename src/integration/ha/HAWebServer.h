#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>

class DeviceConfig;
class DeviceStats;

// Result structure for business logic operations
struct HAOperationResult {
  bool success = false;
  String errorMessage;
  JsonDocument data;

  HAOperationResult() = default;
  HAOperationResult(bool success) : success(success) {}
  HAOperationResult(bool success, const String &error) : success(success), errorMessage(error) {}
};

class HAWebServer {
public:
  HAWebServer(DeviceConfig &config, DeviceStats &stats, State &state);

  bool init();
  void process();
  void stop();

  // WebSocket communication
  void broadcastStateUpdate(const JsonDocument &stateData);
  void setStatusCallback(std::function<void(JsonObject &)> callback);
  void setStateUpdateCallback(
      std::function<HAOperationResult(const String &, const JsonVariant &)> callback);

private:
  void setupRoutes();
  void setupWebSocket();

  // REST API handlers
  void handleGetTsuryPhoneConfig(AsyncWebServerRequest *request);
  void handleRefetchAll(AsyncWebServerRequest *request);

  // Device operation handlers
  void handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAnswerCall(AsyncWebServerRequest *request);
  void handleHangupCall(AsyncWebServerRequest *request);
  void handleDialQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleToggleCallWaiting(AsyncWebServerRequest *request);

  // Configuration handlers
  void handleSetDND(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json);

  // System control handlers
  void handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json);
  void handleResetDevice(AsyncWebServerRequest *request);

  // Number management handlers
  void handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetHAUrl(AsyncWebServerRequest *request, JsonVariant &json);

  // WebSocket event handlers
  void onWebSocketEvent(AsyncWebSocket *server,
                        AsyncWebSocketClient *client,
                        AwsEventType type,
                        void *arg,
                        uint8_t *data,
                        size_t len);

  // Utility functions
  void
  sendJsonResponse(AsyncWebServerRequest *request, const JsonDocument &doc, int statusCode = 200);
  void sendErrorResponse(AsyncWebServerRequest *request, const String &error, int statusCode = 400);
  bool parseJsonBody(AsyncWebServerRequest *request, JsonVariant &json);

  // Helper functions for hierarchical data building
  void addStatus(JsonObject &root);
  void addConfig(JsonObject &root);
  void addStats(JsonObject &root);
  void addPhone(JsonObject &root);

  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;
  AsyncWebServer _server;
  AsyncWebSocket _webSocket;
  std::function<void(JsonObject &)> _statusCallback;
  std::function<HAOperationResult(const String &, const JsonVariant &)> _stateUpdateCallback;

  // WebSocket cleanup optimization
  unsigned long _lastCleanupTime = 0;
  static const unsigned long kWebSocketCleanupInterval = 60000; // 60 seconds

  static const int kServerPort = 8080;
  static const char *kWebSocketPath;
};

#endif // HOME_ASSISTANT_INTEGRATION
