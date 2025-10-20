#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "../IIntegration.h"
#include "../IntegrationService.h"
#include "../IntegrationTypes.h"
#include "../core/DeviceStats.h"
#include "HAConfig.h"
#include "HANumberHandler.h"
#include "HAWebServer.h"
#include <deque>
#include <functional>

/**
 * Home Assistant integration implementation
 * Thin wrapper around IntegrationService that handles HA-specific protocol concerns
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
  void updateCallInfo(const String &number,
                      bool isIncoming,
                      unsigned long startTime = 0,
                      bool isPriority = false,
                      const String &name = "") override;
  void updateDialingProgress(const String &currentNumber) override;
  void updateRingState(bool isRinging) override;
  void updateSystemStatus() override;
  void updateDndState(bool isDndActive);
  void updateVolumeMode(VolumeMode mode) override;

  // Device operation callbacks (to be called by main application)
  void setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) override;
  void setDialDigitCallback(std::function<IntegrationCallbackResult(uint8_t)> callback) override;
  void setAnswerCallback(std::function<IntegrationCallbackResult()> callback) override;
  void setHangupCallback(std::function<IntegrationCallbackResult()> callback) override;
  void
  setRingCallback(std::function<IntegrationCallbackResult(const String &, bool)> callback) override;
  void setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) override;
  void
  setVolumeModeCallback(std::function<IntegrationCallbackResult(VolumeMode)> callback) override;
  void setMaintenanceModeChangedCallback(std::function<void(bool)> callback) override;
  void setFactoryResetCallback(std::function<void()> callback) override;

  // Statistics and monitoring
  void reportCallStart(const String &number, bool isIncoming) override;
  void reportCallEnd(unsigned long duration) override;
  void reportBlockedCall(const String &number) override;
  void reportError(const String &error) override;
  void triggerAction(const String &actionId) override; // Generic action (HA webhooks)

  // Configuration synchronization
  void onConfigurationChanged() override;

  // HA-specific command handlers (thin wrappers around business logic)
  HAOperationResult handleDialRequest(const String &number);
  HAOperationResult handleDialDigitRequest(const JsonVariant &data);
  HAOperationResult handleAnswerRequest();
  HAOperationResult handleHangupRequest();
  HAOperationResult handleSetDND(const JsonVariant &data);
  HAOperationResult handleSetMaintenanceMode(const JsonVariant &data);
  HAOperationResult handleSetAudioConfig(const JsonVariant &data);
  HAOperationResult handleSetRingPattern(const JsonVariant &data);
  HAOperationResult handleSetDialingConfig(const JsonVariant &data);
  HAOperationResult handleRingOperation(const JsonVariant &data);
  HAOperationResult handleSetVolumeMode(const JsonVariant &data);
  HAOperationResult handleResetDevice();
  HAOperationResult handleFactoryReset();
  HAOperationResult handleAddQuickDial(const JsonVariant &data);
  HAOperationResult handleRemoveQuickDial(const JsonVariant &data);
  HAOperationResult handleDialQuickDial(const JsonVariant &data);
  HAOperationResult handleToggleCallWaiting();
  HAOperationResult handleAddBlockedNumber(const JsonVariant &data);
  HAOperationResult handleRemoveBlockedNumber(const JsonVariant &data);
  HAOperationResult handleAddPriorityCaller(const JsonVariant &data);
  HAOperationResult handleRemovePriorityCaller(const JsonVariant &data);
  HAOperationResult handleAddWebhookAction(const JsonVariant &data);
  HAOperationResult handleRemoveWebhookAction(const JsonVariant &data);
  HAOperationResult handleSetHAUrl(const JsonVariant &data);
  HAOperationResult handleGetTsuryPhoneConfig();
  HAOperationResult handleRefetchAll();

  // IIntegration interface implementation
  const char *getName() const override {
    return "HomeAssistant";
  }
  bool isEnabled() const override {
    return true;
  }
  const char *getTag() const override {
    return "HA";
  }
  uint32_t getCapabilities() const override {
    return IC_ACTIONS; // HA supports action triggering (webhooks mapped to actions)
  }
  void registerActionHandlers(IntegrationManager &manager) override;

  // Home Assistant configuration
  String getHomeAssistantUrl() const {
    return _haConfig.getHomeAssistantUrl();
  }

  // HA-specific number validation
  bool isWebhookTrigger(const String &number) const {
    return _haNumberHandler.isWebhookTrigger(number);
  }

  String getWebhookId(const String &code) const {
    return _haNumberHandler.getWebhookId(code);
  }

  bool isPartialWebhookMatch(const String &dialedNumber) const {
    return _haNumberHandler.isPartialWebhookMatch(dialedNumber);
  }

private:
  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;
  HAWebServer _webServer;
  // IntegrationService instance (can be shared in future via singleton if memory pressure)
  IntegrationService &_integrationService; // (N1) shared singleton instance
  HAConfig _haConfig;
  HANumberHandler _haNumberHandler;

  // Internal methods
  void setupWebServerCallbacks();
  void broadcastFullState();
  void broadcastStateChange(const String &key, const JsonVariant &value);
  // Overloads for primitive convenience (wrap into temporary JsonDocument)
  void broadcastStateChange(const String &key, const String &value);
  void broadcastStateChange(const String &key, bool value);
  void broadcastStateChange(const String &key, int value);
  bool deliverWebhook(const String &actionId, uint8_t attempt);
  void enqueueWebhookRetry(const String &actionId, uint8_t attempt, unsigned long delayMs);
  void processPendingWebhooks();

  // Last update tracking for efficiency
  unsigned long _lastStatsUpdate = 0;
  unsigned long _lastSystemUpdate = 0;
  static const unsigned long kStatsUpdateInterval = 60000;  // 60 seconds
  static const unsigned long kSystemUpdateInterval = 60000; // 60 seconds
  uint32_t _lastStatsHash = 0; // I1: cache of last emitted stats fingerprint

  struct PendingWebhook {
    String actionId;
    uint8_t attempt;
    unsigned long nextAttempt;
  };

  static constexpr uint8_t kMaxWebhookAttempts = 6;
  static constexpr unsigned long kWebhookRetryBaseMs = 5000;
  static constexpr unsigned long kWebhookRetryMaxDelayMs = 60000;
  static constexpr size_t kMaxPendingWebhooks = 10;

  std::deque<PendingWebhook> _pendingWebhooks;
  unsigned long _lastWebhookProcess = 0;

  // Helper to convert IntegrationCallbackResult to HAOperationResult
  HAOperationResult convertResult(const IntegrationCallbackResult &result);
};

#endif // HOME_ASSISTANT_INTEGRATION
