#pragma once

#include "config.h"
#include "state.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

#ifdef HOME_ASSISTANT_INTEGRATION

class HomeAssistantServer {
public:
  HomeAssistantServer();

  void init();
  void process();
  void updateState(const State &state);
  void notifyBlockedCall(const char *number);  // Notify HA when a call is blocked
  void broadcastStateUpdate();  // Broadcast state changes via WebSocket

private:
  void setupRoutes();
  void setupWebSocket();
  void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
  void handleRoot(AsyncWebServerRequest *request);
  void handleStatus(AsyncWebServerRequest *request);
  void handleAction(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleRing(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleMaintenanceMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleDnd(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handlePhoneBook(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleBlockedNumbers(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleStats(AsyncWebServerRequest *request);

  String getStatusJson();
  String getStatsJson();
  String getPhoneBookJson();
  String getBlockedNumbersJson();
  String getDndConfigJson();

  void sendJsonResponse(AsyncWebServerRequest *request, const String &json, int code = 200);
  void sendErrorResponse(AsyncWebServerRequest *request, const String &error, int code = 400);

  AsyncWebServer _server;
  AsyncWebSocket _ws;
  State _lastState;
  uint32_t _uptime;
  uint32_t _totalCalls;
  uint32_t _totalIncomingCalls;
  uint32_t _totalOutgoingCalls;
  uint32_t _totalBlockedCalls;
  uint32_t _totalResets;
  bool _isInitialized;
  bool _stateChanged;
};

extern HomeAssistantServer haServer;

// Utility functions for integration with rest of codebase
bool isNumberBlocked(const char *number);
bool isDndConfigEnabled();
bool isDndForceEnabled();
bool isDndScheduleEnabled();
void getHaDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute);

// HA Phonebook functions for runtime selection
bool isHaPhoneBookEntry(const char *number);
bool isPartialOfHaPhoneBookEntry(const char *number);
const char *getHaPhoneBookNumberForEntry(const char *entry);

#endif // HOME_ASSISTANT_INTEGRATION
