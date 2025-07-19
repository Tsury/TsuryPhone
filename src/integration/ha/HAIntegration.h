#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "../../core/DeviceStats.h"
#include "../IIntegration.h"
#include "HAWebServer.h"
#include <functional>

/**
 * Home Assistant integration implementation
 * Manages all HA-related functionality including web server, state synchronization,
 * and communication with the main application
 */
class HAIntegration : public IIntegration {
public:
  HAIntegration(DeviceConfig &config, DeviceStats &stats, State &state);

  // Lifecycle management
  bool init() override;
  void process() override;
  void stop() override;

  // State synchronization
  void updatePhoneState(AppState newState, AppState previousState) override;
  void updateCallInfo(const String &number, bool isIncoming, unsigned long startTime = 0) override;
  void updateDialingProgress(const String &currentNumber) override;
  void updateRingState(bool isRinging) override;
  void updateSystemStatus() override;
  void updateDndState(bool isDndActive);

  // Device operation callbacks (to be called by main application)
  void setDialCallback(std::function<bool(const String &)> callback) override;
  void setAnswerCallback(std::function<bool()> callback) override;
  void setHangupCallback(std::function<bool()> callback) override;
  void setRingCallback(std::function<bool(const String &)> callback) override;
  void setCallWaitingCallback(std::function<bool()> callback) override;
  void setMaintenanceModeChangedCallback(std::function<void(bool)> callback) override;

  // Statistics and monitoring
  void reportCallStart(const String &number, bool isIncoming) override;
  void reportCallEnd(unsigned long duration) override;
  void reportBlockedCall(const String &number) override;
  void reportError(const String &error) override;
  void triggerWebhook(const String &webhookId) override;

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
  String getHomeAssistantUrl() const {
    return _config.getHomeAssistantUrl();
  }

private:
  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;
  HAWebServer _webServer;

  // Remove duplicated state tracking - use _state reference instead
  // Keep only essential additional information not available in main State
  struct CallInfo {
    String number;
    bool isIncoming = false;
    unsigned long startTime = 0;
  } _currentCall;

  String _currentDialingNumber; // Keep for dialing progress

  // Callback functions for device operations
  std::function<bool(const String &)> _dialCallback;
  std::function<bool()> _answerCallback;
  std::function<bool()> _hangupCallback;
  std::function<bool(const String &)> _ringCallback;
  std::function<bool()> _callWaitingCallback;
  std::function<void(bool)> _maintenanceModeChangedCallback;

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
  void addStatsInfo(JsonObject &obj);
  JsonObject createEventObject(JsonDocument &doc, const String &event, const String &type);

  // Last update tracking for efficiency
  unsigned long _lastStatsUpdate = 0;
  unsigned long _lastSystemUpdate = 0;
  static const unsigned long kStatsUpdateInterval = 60000;  // 60 seconds
  static const unsigned long kSystemUpdateInterval = 60000; // 60 seconds

  // Webhook HTTP functionality
  void triggerWebhookHttp(const String &webhookId);
};

#endif // HOME_ASSISTANT_INTEGRATION
