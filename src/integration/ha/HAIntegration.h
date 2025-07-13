#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "../../core/DeviceStats.h"
#include "../IIntegration.h"
#include "HAWebServer.h"
#include <functional>

// Use AppState from the actual state.h file
using PhoneState = AppState;

/**
 * Home Assistant integration implementation
 * Manages all HA-related functionality including web server, state synchronization,
 * and communication with the main application
 */
class HAIntegration : public IIntegration {
public:
  HAIntegration(DeviceConfig &config, DeviceStats &stats);

  // Lifecycle management
  bool init() override;
  void process() override;
  void stop() override;

  // State synchronization
  void updatePhoneState(PhoneState newState, PhoneState previousState) override;
  void updateCallInfo(const String &number, bool isIncoming, unsigned long startTime = 0) override;
  void updateDialingProgress(const String &currentNumber) override;
  void updateRingState(bool isRinging) override;
  void updateSystemStatus() override;

  // Device operation callbacks (to be called by main application)
  void setDialCallback(std::function<bool(const String &)> callback) override;
  void setAnswerCallback(std::function<bool()> callback) override;
  void setHangupCallback(std::function<bool()> callback) override;
  void setRingCallback(std::function<bool(const String &)> callback) override;
  void setWebhookCallback(std::function<bool(const String &)> callback) override;
  void setCallWaitingCallback(std::function<bool()> callback) override;

  // Statistics and monitoring
  void reportCallStart(const String &number, bool isIncoming) override;
  void reportCallEnd(unsigned long duration) override;
  void reportBlockedCall(const String &number) override;
  void reportError(const String &error) override;
  void reportWebhookTrigger(const String &webhookId) override;

  // Configuration synchronization
  void onConfigurationChanged() override;

  // IIntegration interface implementation
  const char *getName() const override {
    return "HomeAssistant";
  }
  bool isEnabled() const override {
    return true;
  }

  // Status information for web server
  void getFullStatus(JsonObject &obj);

  // Home Assistant configuration
  void setHomeAssistantUrl(const String &url) {
    _homeAssistantUrl = url;
  }
  String getHomeAssistantUrl() const {
    return _homeAssistantUrl;
  }

private:
  DeviceConfig &_config;
  DeviceStats &_stats;
  HAWebServer _webServer;

  // Current state tracking
  PhoneState _currentState = PhoneState::Idle;
  String _currentCallNumber;
  bool _currentCallIsIncoming = false;
  unsigned long _currentCallStartTime = 0;
  String _currentDialingNumber;
  bool _isRinging = false;

  // Callback functions for device operations
  std::function<bool(const String &)> _dialCallback;
  std::function<bool()> _answerCallback;
  std::function<bool()> _hangupCallback;
  std::function<bool(const String &)> _ringCallback;
  std::function<bool(const String &)> _webhookCallback;
  std::function<bool()> _callWaitingCallback;

  // Internal methods
  void setupWebServerCallbacks();
  void broadcastFullState();
  void broadcastStateChange(const String &key, const JsonVariant &value);
  void handleWebServerCommand(const String &command, const JsonVariant &data);

  // JSON helpers
  void addBasicDeviceInfo(JsonObject &obj);
  void addPhoneStateInfo(JsonObject &obj);
  void addCallInfo(JsonObject &obj);
  void addSystemInfo(JsonObject &obj);

  // Last update tracking for efficiency
  unsigned long _lastStatsUpdate = 0;
  unsigned long _lastSystemUpdate = 0;
  static const unsigned long kStatsUpdateInterval = 30000; // 30 seconds
  static const unsigned long kSystemUpdateInterval = 5000; // 5 seconds

  // Home Assistant configuration
  String _homeAssistantUrl = "http://homeassistant.local:8123";

  // Webhook HTTP functionality
  void triggerWebhookHttp(const String &webhookId);
};

#endif // HOME_ASSISTANT_INTEGRATION
