#ifdef HOME_ASSISTANT_INTEGRATION

#include "HAIntegration.h"
#include "../../common/logger.h"
#include "../../common/timeManager.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Helper function to get state name
const char *getStateName(AppState state) {
  switch (state) {
  case AppState::Startup:
    return "Startup";
  case AppState::CheckHardware:
    return "CheckHardware";
  case AppState::CheckLine:
    return "CheckLine";
  case AppState::Idle:
    return "Idle";
  case AppState::InvalidNumber:
    return "InvalidNumber";
  case AppState::IncomingCall:
    return "IncomingCall";
  case AppState::IncomingCallRing:
    return "IncomingCallRing";
  case AppState::InCall:
    return "InCall";
  case AppState::Dialing:
    return "Dialing";
  default:
    return "Unknown";
  }
}

HAIntegration::HAIntegration(DeviceConfig &config, DeviceStats &stats, State &state)
    : _config(config), _stats(stats), _state(state), _webServer(config, stats, state) {}

bool HAIntegration::init() {
  Logger::infoln(F("Initializing Home Assistant integration..."));

  setupWebServerCallbacks();

  if (!_webServer.init()) {
    Logger::errorln(F("Failed to initialize HA web server"));
    return false;
  }

  // Broadcast initial state
  broadcastFullState();

  Logger::infoln(F("Home Assistant integration initialized successfully"));
  return true;
}

void HAIntegration::process() {
  _webServer.process();

  // Periodic updates
  unsigned long now = millis();

  // Update statistics periodically
  if (now - _lastStatsUpdate >= kStatsUpdateInterval) {
    _lastStatsUpdate = now;
    JsonDocument doc;
    JsonObject obj = createEventObject(doc, "system", "stats");
    addStatsInfo(obj);
    _webServer.broadcastStateUpdate(doc);
  }

  // Update system status periodically
  if (now - _lastSystemUpdate >= kSystemUpdateInterval) {
    _lastSystemUpdate = now;
    updateSystemStatus();
  }
}

void HAIntegration::stop() {
  _webServer.stop();
  Logger::infoln(F("Home Assistant integration stopped"));
}

void HAIntegration::updatePhoneState(AppState newState, AppState previousState) {
  Logger::infoln(F("HA: Phone state changed from %s to %s"),
                 getStateName(previousState),
                 getStateName(newState));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "state");
  obj["state"] = static_cast<int>(newState);
  obj["previousState"] = static_cast<int>(previousState);
  obj["stateName"] = getStateName(newState);

  addPhoneStateInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateCallInfo(const String &number, bool isIncoming, unsigned long startTime) {
  _currentCall.number = number;
  _currentCall.isIncoming = isIncoming;
  _currentCall.startTime = startTime > 0 ? startTime : millis();

  Logger::infoln(F("HA: Call info updated - %s call to/from %s"),
                 isIncoming ? F("Incoming") : F("Outgoing"),
                 number.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "call_info");
  addCallInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDialingProgress(const String &currentNumber) {
  if (_currentDialingNumber != currentNumber) {
    _currentDialingNumber = currentNumber;

    Logger::infoln(F("HA: Dialing progress - current number: %s"), currentNumber.c_str());

    JsonDocument doc;
    JsonObject obj = createEventObject(doc, "phone_state", "dialing");
    obj["currentNumber"] = currentNumber;
    _webServer.broadcastStateUpdate(doc);
  }
}

void HAIntegration::updateRingState(bool isRinging) {
  Logger::infoln(F("HA: Ring state changed - %s"), isRinging ? F("ringing") : F("not ringing"));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "ring");
  obj["isRinging"] = isRinging;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateDndState(bool isDndActive) {
  Logger::infoln(F("HA: DND state updated - %s"), isDndActive ? F("active") : F("inactive"));

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "phone_state", "dnd");
  obj["dndActive"] = isDndActive;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::updateSystemStatus() {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "status");
  addSystemInfo(obj);
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::setDialCallback(std::function<bool(const String &)> callback) {
  _dialCallback = callback;
}

void HAIntegration::setAnswerCallback(std::function<bool()> callback) {
  _answerCallback = callback;
}

void HAIntegration::setHangupCallback(std::function<bool()> callback) {
  _hangupCallback = callback;
}

void HAIntegration::setRingCallback(std::function<bool(const String &)> callback) {
  _ringCallback = callback;
}

void HAIntegration::setCallWaitingCallback(std::function<bool()> callback) {
  _callWaitingCallback = callback;
}

void HAIntegration::setMaintenanceModeChangedCallback(std::function<void(bool)> callback) {
  _maintenanceModeChangedCallback = callback;
}

void HAIntegration::reportCallStart(const String &number, bool isIncoming) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to update call info and broadcast to HA
  updateCallInfo(number, isIncoming);

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "start");
  obj["number"] = number;
  obj["isIncoming"] = isIncoming;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportCallEnd(unsigned long duration) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to broadcast to HA
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "end");
  obj["duration"] = duration;
  _webServer.broadcastStateUpdate(doc);

  // Clear call info
  _currentCall.number = "";
  _currentCall.isIncoming = false;
  _currentCall.startTime = 0;
}

void HAIntegration::reportBlockedCall(const String &number) {
  // Note: Stats recording is now handled by StatsManager automatically
  // This method only needs to broadcast to HA
  Logger::infoln(F("HA: Blocked call from %s"), number.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "call", "blocked");
  obj["number"] = number;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::reportError(const String &error) {
  Logger::errorln(F("HA: Error reported - %s"), error.c_str());

  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "error");
  obj["error"] = error;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::onConfigurationChanged() {
  Logger::infoln(F("HA: Configuration changed, broadcasting update"));
  broadcastFullState();
}

void HAIntegration::setupWebServerCallbacks() {
  _webServer.setStatusCallback([this](JsonObject &obj) { getFullStatus(obj); });

  _webServer.setStateUpdateCallback([this](const String &command, const JsonVariant &data) {
    handleWebServerCommand(command, data);
  });
}

void HAIntegration::broadcastFullState() {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "full_state", "");
  addBasicDeviceInfo(obj);
  addPhoneStateInfo(obj);
  addCallInfo(obj);
  addSystemInfo(obj);
  addStatsInfo(obj);

  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::broadcastStateChange(const String &key, const JsonVariant &value) {
  JsonDocument doc;
  JsonObject obj = createEventObject(doc, "system", "config");
  obj[key] = value;
  _webServer.broadcastStateUpdate(doc);
}

void HAIntegration::handleWebServerCommand(const String &command, const JsonVariant &data) {
  Logger::infoln(F("HA: Received command: %s"), command.c_str());

  if (command == "dial" && _dialCallback) {
    String number = data["number"].as<String>();
    if (_dialCallback(number)) {
      Logger::infoln(F("HA: Dial command successful for %s"), number.c_str());
    } else {
      Logger::errorln(F("HA: Dial command failed for %s"), number.c_str());
    }
  } else if (command == "answer" && _answerCallback) {
    if (_answerCallback()) {
      Logger::infoln(F("HA: Answer command successful"));
    } else {
      Logger::errorln(F("HA: Answer command failed"));
    }
  } else if (command == "hangup" && _hangupCallback) {
    if (_hangupCallback()) {
      Logger::infoln(F("HA: Hangup command successful"));
    } else {
      Logger::errorln(F("HA: Hangup command failed"));
    }
  } else if (command == "ring" && _ringCallback) {
    String pattern = data["pattern"].as<String>();
    if (_ringCallback(pattern)) {
      Logger::infoln(F("HA: Ring command successful with pattern %s"), pattern.c_str());
    } else {
      Logger::errorln(F("HA: Ring command failed"));
    }
  } else if (command == "switch_call_waiting") {
    // Handle call waiting toggle
    Logger::infoln(F("HA: Call waiting toggle requested"));
    if (_callWaitingCallback) {
      bool success = _callWaitingCallback();
      if (success) {
        Logger::infoln(F("HA: Call waiting switch successful"));
      } else {
        Logger::errorln(F("HA: Call waiting switch failed"));
      }
    } else {
      Logger::errorln(F("HA: No callback available for call waiting switch"));
    }
  } else if (command == "refresh") {
    broadcastFullState();
  } else if (command == "maintenance_mode" && _maintenanceModeChangedCallback) {
    bool enabled = data["enabled"].as<bool>();
    Logger::infoln(F("HA: Maintenance mode %s"), enabled ? F("enabled") : F("disabled"));
    _maintenanceModeChangedCallback(enabled);
  } else {
    Logger::warnln(F("HA: Unknown command: %s"), command.c_str());
  }
}

void HAIntegration::addBasicDeviceInfo(JsonObject &obj) {
  obj["deviceName"] = _config.getDeviceName();
  obj["deviceId"] = _config.getDeviceId();
}

void HAIntegration::addPhoneStateInfo(JsonObject &obj) {
  obj["state"] = static_cast<int>(_state.newAppState);
  obj["stateName"] = getStateName(_state.newAppState);
  obj["dndActive"] = _state.isDnd;
  obj["maintenanceMode"] = _state.isMaintenanceMode;

  // Add call state information
  if (_state.callState.callNumber[0] != '\0') {
    obj["currentDialingNumber"] = _state.callState.callNumber;
  }
}

void HAIntegration::addCallInfo(JsonObject &obj) {
  if (!_currentCall.number.isEmpty()) {
    obj["currentCallNumber"] = _currentCall.number;
    obj["currentCallIsIncoming"] = _currentCall.isIncoming;
    obj["currentCallStartTime"] = _currentCall.startTime;

    if (_currentCall.startTime > 0) {
      obj["currentCallDuration"] = (millis() - _currentCall.startTime) / 1000;
    }
  }
}

void HAIntegration::addSystemInfo(JsonObject &obj) {
  obj["freeHeap"] = _stats.getFreeHeap();
  obj["rssi"] = _stats.getRSSI();
  obj["timestamp"] = millis(); // Use millis() instead of getUnixTime()
  obj["uptime"] = _stats.getUptime();
}

void HAIntegration::addStatsInfo(JsonObject &obj) {
  const CallStats &callStats = _stats.getCallStats();
  obj["totalCalls"] = callStats.totalCalls;
  obj["incomingCalls"] = callStats.incomingCalls;
  obj["outgoingCalls"] = callStats.outgoingCalls;
  obj["blockedCalls"] = callStats.blockedCalls;
  obj["totalTalkTimeSeconds"] = callStats.totalTalkTimeSeconds;
}

void HAIntegration::getFullStatus(JsonObject &obj) {
  // Add basic device info
  addBasicDeviceInfo(obj);

  // Add phone state info
  addPhoneStateInfo(obj);

  // Add call info
  addCallInfo(obj);

  // Add system info
  addSystemInfo(obj);

  // Add stats info
  addStatsInfo(obj);
}

// Helper function to create WebSocket event objects
JsonObject
HAIntegration::createEventObject(JsonDocument &doc, const String &event, const String &type) {
  JsonObject obj = doc.to<JsonObject>();
  obj["event"] = event;
  obj["type"] = type;
  obj["timestamp"] = millis();
  return obj;
}

void HAIntegration::triggerWebhookHttp(const String &webhookId) {
  // Only trigger if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Logger::warnln(F("HA: Webhook trigger skipped - WiFi not connected"));
    return;
  }

  // Construct webhook URL
  String webhookUrl = _config.getHomeAssistantUrl() + "/api/webhook/" + webhookId;

  Logger::infoln(F("HA: Triggering webhook HTTP call: %s"), webhookUrl.c_str());

  HTTPClient http;
  http.begin(webhookUrl);
  http.setTimeout(5000); // 5 second timeout

  // Make POST request (webhooks typically expect POST)
  int httpResponseCode = http.POST("");

  if (httpResponseCode > 0) {
    Logger::infoln(F("HA: Webhook HTTP response: %d"), httpResponseCode);
    if (httpResponseCode == 200) {
      Logger::infoln(F("HA: Webhook triggered successfully"));
    }
  } else {
    Logger::errorln(F("HA: Webhook HTTP error: %s"), http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

void HAIntegration::triggerWebhook(const String &webhookId) {
  Logger::infoln(F("HA: Triggering webhook - %s"), webhookId.c_str());

  // Trigger the actual HTTP webhook call to Home Assistant
  triggerWebhookHttp(webhookId);
}

#endif // HOME_ASSISTANT_INTEGRATION
