#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../config/configProvider.h"
#include "../core/phoneController.h"
#include "../core/state.h"
#include "../hardware/ringer.h"
#include "config.h"
#include "server.h"
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>

// Webhook shortcuts for Home Assistant integration
struct WebhookEntry {
  String number;
  String webhookId;
};

// Home Assistant configuration provider implementation
class HaConfigProvider : public IConfigProvider {
public:
  // DnD configuration from HA settings
  bool isDndConfigEnabled() const override;
  bool isDndForceEnabled() const override;
  bool isDndScheduleEnabled() const override;
  void getDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute) const override;

  // Runtime DnD hour override
  void setDndHoursOverride(int startHour, int startMinute, int endHour, int endMinute) override;
  bool hasDndHoursOverride() const override;
  void clearDndHoursOverride() override;

  // Blocking functionality from HA settings
  bool isNumberBlocked(const char *number) const override;

  // Webhook functionality from HA settings
  bool isWebhookEntry(const char *number) const override;
  bool isPartialOfWebhookEntry(const char *number) const override;
  const char *getWebhookIdForNumber(const char *number) const override;
  void executeWebhook(const char *webhookId) const override;

  // HA-specific internal phonebook methods (not part of IConfigProvider interface)
  bool isHaPhoneBookEntry(const char *number) const;
  bool isPartialOfHaPhoneBookEntry(const char *number) const;
  const char *getHaPhoneBookNumberForEntry(const char *entry) const;

private:
  // Runtime DnD hour override storage
  mutable int _overrideStartHour = -1;
  mutable int _overrideStartMinute = -1;
  mutable int _overrideEndHour = -1;
  mutable int _overrideEndMinute = -1;
};

class HomeAssistantServer : public IServer {
public:
  HomeAssistantServer();

  // IServer interface implementation
  void init() override;
  void setPhoneController(IPhoneController *controller) override;
  void process(const State &state) override;
  const char *getName() const override {
    return "HomeAssistant";
  }
  bool isEnabled() const override;
  void notifyBlockedCall(const char *number) override;

  // Webhook methods
  bool hasWebhookEntry(const char *number) const override;
  bool hasPartialWebhookEntry(const char *number) const override;
  bool executeWebhook(const char *number) const override;

  // HA-specific functionality
  void broadcastStateUpdate();

  // Allow HaConfigProvider to access internal data
  friend class HaConfigProvider;

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
  IPhoneController *_phoneController; // Dependency injection instead of global functions
};

extern HaConfigProvider haConfigProvider;

// Webhook management functions (internal use)
void addWebhookEntry(const char *number, const char *webhookId);
void removeWebhookEntry(const char *number);

#endif // HOME_ASSISTANT_INTEGRATION
