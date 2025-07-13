#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include "../../common/state.h"

class DeviceConfig;
class DeviceStats;

class HAWebServer {
public:
  HAWebServer(DeviceConfig &config, DeviceStats &stats);

  bool init();
  void process();
  void stop();

  // WebSocket communication
  void broadcastStateUpdate(const JsonDocument &stateData);
  void setStatusCallback(std::function<void(JsonObject &)> callback);
  void setStateUpdateCallback(std::function<void(const String &, const JsonVariant &)> callback);

private:
  void setupRoutes();
  void setupWebSocket();

  // REST API handlers
  void handleGetStatus(AsyncWebServerRequest *request);
  void handleGetConfig(AsyncWebServerRequest *request);
  void handlePostConfig(AsyncWebServerRequest *request, JsonVariant &json);
  void handleGetStats(AsyncWebServerRequest *request);

  // Device operation handlers
  void handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAnswerCall(AsyncWebServerRequest *request);
  void handleHangupCall(AsyncWebServerRequest *request);
  void handleSetDND(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetDNDSchedule(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetDNDStartTime(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetDNDEndTime(AsyncWebServerRequest *request, JsonVariant &json);
  void handleDialQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json);
  void handleResetDevice(AsyncWebServerRequest *request);
  void handleRefetchData(AsyncWebServerRequest *request);
  void handleToggleCallWaiting(AsyncWebServerRequest *request);

  // Number management handlers
  void handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);

  // Audio settings handlers
  void handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json);

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

  DeviceConfig &_config;
  DeviceStats &_stats;
  AsyncWebServer _server;
  AsyncWebSocket _webSocket;
  std::function<void(JsonObject &)> _statusCallback;
  std::function<void(const String &, const JsonVariant &)> _stateUpdateCallback;

  static const int kServerPort = 8080;
  static const char *kWebSocketPath;
};

#endif // HOME_ASSISTANT_INTEGRATION
