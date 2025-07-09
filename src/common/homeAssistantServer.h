#pragma once

#include "../components/ringer.h"
#include "config.h"
#include "state.h"
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>

#ifdef HOME_ASSISTANT_INTEGRATION

// Webhook shortcuts for Home Assistant integration
struct WebhookEntry {
  String number;
  String webhookId;
};

class HomeAssistantServer {
public:
  HomeAssistantServer();
  void init();
  void process(const State &state);
  void notifyBlockedCall(const char *number);
  void broadcastStateUpdate();

private:
  void setupRoutes();
  void setupWebSocket();
  void onWebSocketEvent(AsyncWebSocket *server,
                        AsyncWebSocketClient *client,
                        AwsEventType type,
                        void *arg,
                        uint8_t *data,
                        size_t len);

  // Simplified handlers - combine similar functionality
  void handleRequest(AsyncWebServerRequest *request, const char *endpoint);
  void handlePostRequest(AsyncWebServerRequest *request,
                         uint8_t *data,
                         size_t len,
                         const char *endpoint);

  // JSON response helpers - use static strings when possible
  void sendResponse(AsyncWebServerRequest *request, const char *data, int code = 200);
  void sendError(AsyncWebServerRequest *request, const char *error, int code = 400);

  // Ring pattern parsing
  RingPattern parseRingPattern(const String &pattern);

  AsyncWebServer _server;
  AsyncWebSocket _ws;
  State _lastState;
  uint32_t _uptime;
  uint32_t _stats[6]; // [calls, incoming, outgoing, blocked, resets, maintenance_events]
  bool _isInitialized;
  bool _stateChanged;
};

extern HomeAssistantServer haServer;

// Utility functions
bool isNumberBlocked(const char *number);
bool isDndConfigEnabled();
bool isDndForceEnabled();
bool isDndScheduleEnabled();
void getHaDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute);

// HA Phonebook functions
bool isHaPhoneBookEntry(const char *number);
bool isPartialOfHaPhoneBookEntry(const char *number);
const char *getHaPhoneBookNumberForEntry(const char *entry);

// Webhook shortcuts functions
bool isWebhookEntry(const char *number);
bool isPartialOfWebhookEntry(const char *number);
const char *getWebhookIdForNumber(const char *number);
void addWebhookEntry(const char *number, const char *webhookId);
void removeWebhookEntry(const char *number);
void executeWebhook(const char *webhookId);

#endif // HOME_ASSISTANT_INTEGRATION
