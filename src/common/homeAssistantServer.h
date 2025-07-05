#pragma once

#include "config.h"
#include "state.h"
#include <ESPAsyncWebServer.h>

#ifdef HOME_ASSISTANT_INTEGRATION

class HomeAssistantServer {
public:
  HomeAssistantServer()
      : _server(80),
        _uptime(0),
        _totalCalls(0),
        _totalIncomingCalls(0),
        _totalOutgoingCalls(0),
        _totalResets(0),
        _isInitialized(false) {}

  void init();
  void process();
  void updateState(const State &state);

  // Configuration methods
  void setDndEnabled(bool enabled);
  void setDndHours(int startHour, int startMinute, int endHour, int endMinute);
  void addPhoneBookEntry(const char *name, const char *number);
  void removePhoneBookEntry(const char *name);
  void addScreenedNumber(const char *number);
  void removeScreenedNumber(const char *number);

  // Action methods
  void performCall(const char *number);
  void performHangup();
  void performReset();
  void performFactoryReset();
  void performReboot();

private:
  void setupRoutes();
  void handleRoot(AsyncWebServerRequest *request);
  void handleStatus(AsyncWebServerRequest *request);
  void handleAction(AsyncWebServerRequest *request);
  void handleConfig(AsyncWebServerRequest *request);
  void handlePhoneBook(AsyncWebServerRequest *request);
  void handleScreenedNumbers(AsyncWebServerRequest *request);
  void handleDnd(AsyncWebServerRequest *request);
  void handleStats(AsyncWebServerRequest *request);

  String getStatusJson();
  String getStatsJson();
  String getPhoneBookJson();
  String getScreenedNumbersJson();
  String getDndConfigJson();

  void sendJsonResponse(AsyncWebServerRequest *request, const String &json, int code = 200);
  void sendErrorResponse(AsyncWebServerRequest *request, const String &error, int code = 400);

  AsyncWebServer _server;
  State _lastState;
  uint32_t _uptime;
  uint32_t _totalCalls;
  uint32_t _totalIncomingCalls;
  uint32_t _totalOutgoingCalls;
  uint32_t _totalResets;
  bool _isInitialized;
};

extern HomeAssistantServer haServer;

// Utility functions for integration with rest of codebase
bool isNumberScreened(const char *number);
bool isDndConfigEnabled();
void getHaDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute);

// HA Phonebook functions for runtime selection
bool isHaPhoneBookEntry(const char *number);
bool isPartialOfHaPhoneBookEntry(const char *number);
const char *getHaPhoneBookNumberForEntry(const char *entry);

#endif // HOME_ASSISTANT_INTEGRATION
