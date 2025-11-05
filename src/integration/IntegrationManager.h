#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../common/state.h"
#include "../core/DeviceConfig.h"
#include "ActionHandler.h"
#include "IIntegration.h"
#include "IntegrationTypes.h"
#include "core/DeviceStats.h"
#include "core/StatsManager.h"
#include <ArduinoJson.h>
#include <functional>
#include <memory>
#include <vector>

// Forward declaration
class TsuryPhone;
struct IntegrationCallbackResult;

/**
 * Manager for all device integrations
 * Uses a collection-based approach to support multiple integrations simultaneously
 * Each integration implements the IIntegration interface
 */
class IntegrationManager {
public:
  IntegrationManager(TsuryPhone &tsuryPhone, DeviceConfig &config, State &state);
  ~IntegrationManager();

  // Lifecycle management
  bool init();
  void process();
  void stop();

  // Setup callbacks - to be called after construction to avoid circular dependencies
  void setupTsuryPhoneCallbacks(
      std::function<IntegrationCallbackResult(const String &)> dialCallback,
      std::function<IntegrationCallbackResult(uint8_t, bool)> dialDigitCallback,
      std::function<IntegrationCallbackResult()> deleteLastDigitCallback,
      std::function<IntegrationCallbackResult()> sendDialedNumberCallback,
      std::function<IntegrationCallbackResult()> answerCallback,
      std::function<IntegrationCallbackResult()> hangupCallback,
      std::function<IntegrationCallbackResult(const String &, bool)> ringCallback,
      std::function<IntegrationCallbackResult()> callWaitingCallback,
      std::function<IntegrationCallbackResult(VolumeMode)> volumeModeCallback,
      std::function<IntegrationCallbackResult()> toggleMuteCallback,
      std::function<void(const String &)> callBlockedCallback,
      std::function<void(bool)> maintenanceModeCallback,
      std::function<void()> factoryResetCallback,
      std::function<void(ConfigChangeEvent)> configChangeCallback);

  // State synchronization - calls all registered integrations
  void updatePhoneState(AppState newState, AppState previousState);
  void updateCallInfo(const String &number,
                      bool isIncoming,
                      unsigned long startTime = 0,
                      bool isPriority = false,
                      const String &name = "");
  void updateDialingProgress(const String &currentNumber);
  void updateSystemStatus();
  void updateDndState(bool isDndActive);
  void updateVolumeMode(VolumeMode mode);

  // Device lifecycle hooks
  void onFactoryResetInitiated();
  void onFactoryResetBeforeRestart();

  // Device operation callbacks - sets callbacks for all integrations
  void setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback);
  void setDialDigitCallback(std::function<IntegrationCallbackResult(uint8_t, bool)> callback);
  void setDeleteLastDigitCallback(std::function<IntegrationCallbackResult()> callback);
  void setSendDialedNumberCallback(std::function<IntegrationCallbackResult()> callback);
  void setAnswerCallback(std::function<IntegrationCallbackResult()> callback);
  void setHangupCallback(std::function<IntegrationCallbackResult()> callback);
  void setRingCallback(std::function<IntegrationCallbackResult(const String &, bool)> callback);
  void setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback);
  void setVolumeModeCallback(std::function<IntegrationCallbackResult(VolumeMode)> callback);
  void setToggleMuteCallback(std::function<IntegrationCallbackResult()> callback);
  void setMaintenanceModeChangedCallback(std::function<void(bool)> callback);
  void setFactoryResetCallback(std::function<void()> callback);

  // Call blocking callback - notifies when a call should be blocked
  void setCallBlockedCallback(std::function<void(const String &)> callback);

  // Statistics and monitoring - reports to all integrations
  void reportCallStart(const String &number, bool isIncoming);
  void reportCallEnd(unsigned long duration);
  void reportBlockedCall(const String &number);
  void reportError(const String &error);
  void triggerAction(const String &actionId);
  // Action handler registry
  void addActionHandler(IIntegrationActionHandler *handler);
  bool hasPartialActionMatch(const String &dialed) const;
  bool isActionCode(const String &dialed) const;
  String resolveActionId(const String &dialed) const;
  // Debug helper: dump all action codes (if handlers enumerate them)
  void listActionCodes() const;
  // N2: Export metrics snapshot (logs concise stats summary)
  void exportMetricsSnapshot() const;
  // N3: Emit a structured JSON log sample (minimal) for external tooling experimentation
  void emitStructuredJsonLog(const char *event, const char *detail) const;
  void emitStructuredJsonLogKV(const char *event, const char *k, const char *v) const;

  // Configuration synchronization - notifies all integrations
  void onConfigurationChanged();

  // Config change event system
  void addConfigChangeCallback(ConfigChangeCallback callback);
  void notifyConfigChange(ConfigChangeEvent event);

  // Management
  bool hasEnabledIntegrations() const;
  void listIntegrations() const;

  // DeviceStats access
  DeviceStats &getDeviceStats() {
    return _stats;
  }
  const DeviceStats &getDeviceStats() const {
    return _stats;
  }

  // Additional state tracking for automatic detection
  void handleCallBlocked(const String &number);
  void handleCallStarted(const String &number, bool isIncoming);
  void handleCallEnded(unsigned long duration);

  // Device config updates - for local changes that should notify integrations
  void updateMaintenanceMode(bool enabled);

  // Runtime logging control for integration debug messages
  void enableIntegrationDebugLogging(bool enabled);

  // Enqueue a debug character from serial input; handled during process()
  void enqueueDebugChar(char c);

  // All transport/protocol specifics are handled in concrete integration classes.

private:
  void updateRingState(bool isRinging);
  void checkForStateChanges(); // New method to track state changes

  TsuryPhone &_tsuryPhone;
  DeviceConfig &_config;
  DeviceStats _stats; // Now owned by IntegrationManager
  State &_state;
  StatsManager _statsManager; // Automatic stats tracking

  // State change tracking
  bool _prevDndState = false;
  bool _prevMaintenanceMode = false;
  bool _prevHookOff = false;
  AppState _prevAppState = AppState::Startup;
  bool _prevRingingState = false;
  VolumeMode _prevVolumeMode = VolumeMode::Earpiece;
  String _prevCallNumber = "";
  String _prevDialingNumber = "";
  String _prevCallStateNumber = ""; // Cache for call state number to avoid string creation
  bool _callWasActive = false;
  unsigned long _callStartTime = 0;
  int _prevCallId = -1;
  int _prevCallWaitingId = -1;
  bool _prevCallWaitingAvailable = false;
  bool _prevCallWaitingOnHold = false;
  unsigned long _lastCallDurationBroadcast = 0;

  // Callback for blocked calls
  std::function<void(const String &)> _callBlockedCallback;

  // Config change callbacks
  std::vector<ConfigChangeCallback> _configChangeCallbacks;

  std::vector<std::unique_ptr<IIntegration>> _integrations;
  std::vector<IIntegrationActionHandler *>
      _actionHandlers; // Non-owning; lifetime managed by integration

  void registerIntegrations();
  mutable uint32_t _jsonLogSeq = 0; // N3 sequence counter
  // Queue for pending debug serial characters to be processed inside process()
  std::vector<char> _debugCharQueue;
};

#endif // HOME_ASSISTANT_INTEGRATION