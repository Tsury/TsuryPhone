#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>

class DeviceConfig;
class DeviceStats;

struct HAOperationResult {
  bool success = false;
  String errorMessage;
  String errorCode; // DS5
  JsonDocument data;

  HAOperationResult() = default;
  HAOperationResult(bool success) : success(success) {}
  HAOperationResult(bool success, const String &error) : success(success), errorMessage(error) {}
  HAOperationResult(bool success, const String &error, const String &code)
      : success(success), errorMessage(error), errorCode(code) {}
};

class HAWebServer {
public:
  HAWebServer(DeviceConfig &config, DeviceStats &stats, State &state);

  bool init();
  void process();
  void stop();

  void broadcastStateUpdate(const JsonDocument &stateData);
  void setStatusCallback(std::function<void(JsonObject &)> callback);
  void setStateUpdateCallback(
      std::function<HAOperationResult(const String &, const JsonVariant &)> callback);

private:
  void setupRoutes();
  void setupWebSocket();
  void setupMDNS();

  void addJsonPostRoute(const String &path,
                        std::function<void(AsyncWebServerRequest *, JsonVariant &)> handler);

  void handleGetTsuryPhoneConfig(AsyncWebServerRequest *request);
  void handleRefetchAll(AsyncWebServerRequest *request);

  void handleDialNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleDialDigit(AsyncWebServerRequest *request, JsonVariant &json);
  void handleDeleteLastDigit(AsyncWebServerRequest *request);
  void handleSendDialedNumber(AsyncWebServerRequest *request);
  void handleAnswerCall(AsyncWebServerRequest *request);
  void handleHangupCall(AsyncWebServerRequest *request);
  void handleDialQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleToggleCallWaiting(AsyncWebServerRequest *request);
  void handleToggleVolumeMode(AsyncWebServerRequest *request);

  void handleSetDND(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetMaintenanceMode(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetAudioConfig(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetRingPattern(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetDialingConfig(AsyncWebServerRequest *request, JsonVariant &json);

  void handleRingOperation(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetVolumeMode(AsyncWebServerRequest *request, JsonVariant &json);
  void handleResetDevice(AsyncWebServerRequest *request);
  void handleFactoryReset(AsyncWebServerRequest *request);

  void handleAddQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveQuickDial(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveBlockedNumber(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddPriorityCaller(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemovePriorityCaller(AsyncWebServerRequest *request, JsonVariant &json);
  void handleAddWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);
  void handleRemoveWebhookAction(AsyncWebServerRequest *request, JsonVariant &json);
  void handleSetHAUrl(AsyncWebServerRequest *request, JsonVariant &json);
  void handleDiagnostics(AsyncWebServerRequest *request); // /api/diagnostics

  void onWebSocketEvent(AsyncWebSocket *server,
                        AsyncWebSocketClient *client,
                        AwsEventType type,
                        void *arg,
                        uint8_t *data,
                        size_t len);

  void
  sendJsonResponse(AsyncWebServerRequest *request, const JsonDocument &doc, int statusCode = 200);
  void sendErrorResponse(AsyncWebServerRequest *request,
                         const String &error,
                         int statusCode = 400,
                         const String &errorCode = String());
  bool parseJsonBody(AsyncWebServerRequest *request, JsonVariant &json);

  void
  executeCommand(AsyncWebServerRequest *request, const String &command, const JsonVariant &data);

  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;
  AsyncWebServer _server;
  AsyncWebSocket _webSocket;
  std::function<void(JsonObject &)> _statusCallback;
  std::function<HAOperationResult(const String &, const JsonVariant &)> _stateUpdateCallback;

  unsigned long _lastCleanupTime = 0;
  static const unsigned long kWebSocketCleanupInterval = 60000;

  static const int kServerPort = 8080;
  static const char *kWebSocketPath;
};

#endif
