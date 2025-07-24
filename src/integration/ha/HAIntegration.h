#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "../../core/DeviceStats.h"
#include "../IIntegration.h"
#include "HAWebServer.h"
#include <functional>

// Forward declaration
enum class ConfigChangeEvent;
using ConfigChangeCallback = std::function<void(ConfigChangeEvent event)>;

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
  void setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) override;
  void setAnswerCallback(std::function<IntegrationCallbackResult()> callback) override;
  void setHangupCallback(std::function<IntegrationCallbackResult()> callback) override;
  void setRingCallback(std::function<IntegrationCallbackResult(const String &)> callback) override;
  void setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) override;
  void setMaintenanceModeChangedCallback(std::function<void(bool)> callback) override;

  // Statistics and monitoring
  void reportCallStart(const String &number, bool isIncoming) override;
  void reportCallEnd(unsigned long duration) override;
  void reportBlockedCall(const String &number) override;
  void reportError(const String &error) override;
  void triggerWebhook(const String &webhookId) override;

  // Configuration synchronization
  void onConfigurationChanged() override;

  // Config change event system
  void setConfigChangeCallback(ConfigChangeCallback callback);

  // Business logic methods for web server
  HAOperationResult handleDialRequest(const String &number);
  HAOperationResult handleAnswerRequest();
  HAOperationResult handleHangupRequest();
  HAOperationResult handleSetDND(const JsonVariant &data);
  HAOperationResult handleSetMaintenanceMode(const JsonVariant &data);
  HAOperationResult handleSetAudioConfig(const JsonVariant &data);
  HAOperationResult handleSetRingPattern(const JsonVariant &data);
  HAOperationResult handleRingOperation(const JsonVariant &data);
  HAOperationResult handleResetDevice();
  HAOperationResult handleAddQuickDial(const JsonVariant &data);
  HAOperationResult handleRemoveQuickDial(const JsonVariant &data);
  HAOperationResult handleDialQuickDial(const JsonVariant &data);
  HAOperationResult handleToggleCallWaiting();
  HAOperationResult handleAddBlockedNumber(const JsonVariant &data);
  HAOperationResult handleRemoveBlockedNumber(const JsonVariant &data);
  HAOperationResult handleAddWebhookAction(const JsonVariant &data);
  HAOperationResult handleRemoveWebhookAction(const JsonVariant &data);
  HAOperationResult handleSetHAUrl(const JsonVariant &data);
  HAOperationResult handleRefetchAll();

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
  std::function<IntegrationCallbackResult(const String &)> _dialCallback;
  std::function<IntegrationCallbackResult()> _answerCallback;
  std::function<IntegrationCallbackResult()> _hangupCallback;
  std::function<IntegrationCallbackResult(const String &)> _ringCallback;
  std::function<IntegrationCallbackResult()> _callWaitingCallback;
  std::function<void(bool)> _maintenanceModeChangedCallback;

  // Config change callback
  ConfigChangeCallback _configChangeCallback;

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
  
  // Reset management
  bool _resetRequested = false;
  unsigned long _resetScheduledTime = 0;
};

#endif // HOME_ASSISTANT_INTEGRATION
