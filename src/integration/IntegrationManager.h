#pragma once

#include "../common/state.h"
#include "../core/DeviceConfig.h"
#include "../core/DeviceStats.h"
#include "../core/StatsManager.h"
#include "IIntegration.h"
#include <functional>
#include <memory>
#include <vector>

/**
 * Manager for all device integrations
 * Uses a collection-based approach to support multiple integrations simultaneously
 * Each integration implements the IIntegration interface
 */
class IntegrationManager {
public:
  IntegrationManager(DeviceConfig &config, DeviceStats &stats, State &state);
  ~IntegrationManager();

  // Lifecycle management
  bool init();
  void process();
  void stop();

  // State synchronization - calls all registered integrations
  void updatePhoneState(AppState newState, AppState previousState);
  void updateCallInfo(const String &number, bool isIncoming, unsigned long startTime = 0);
  void updateDialingProgress(const String &currentNumber);
  void updateSystemStatus();
  void updateDndState(bool isDndActive);

  // Device operation callbacks - sets callbacks for all integrations
  void setDialCallback(std::function<bool(const String &)> callback);
  void setAnswerCallback(std::function<bool()> callback);
  void setHangupCallback(std::function<bool()> callback);
  void setRingCallback(std::function<bool(const String &)> callback);
  void setCallWaitingCallback(std::function<bool()> callback);
  void setMaintenanceModeChangedCallback(std::function<void(bool)> callback);

  // Call blocking callback - notifies when a call should be blocked
  void setCallBlockedCallback(std::function<void(const String &)> callback);

  // Statistics and monitoring - reports to all integrations
  void reportCallStart(const String &number, bool isIncoming);
  void reportCallEnd(unsigned long duration);
  void reportBlockedCall(const String &number);
  void reportError(const String &error);
  void triggerWebhook(const String &webhookId);

  // Configuration synchronization - notifies all integrations
  void onConfigurationChanged();

  // Management
  bool hasEnabledIntegrations() const;
  void listIntegrations() const;

  // Additional state tracking for automatic detection
  void handleCallBlocked(const String &number);
  void handleCallStarted(const String &number, bool isIncoming);
  void handleCallEnded(unsigned long duration);

  // Call blocking support
  bool shouldBlockCall(const String &number) const;

  // Device config updates - for local changes that should notify integrations
  void updateMaintenanceMode(bool enabled);

private:
  void updateRingState(bool isRinging);
  void checkForStateChanges(); // New method to track state changes

  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;
  StatsManager _statsManager; // Automatic stats tracking

  // State change tracking
  bool _prevDndState = false;
  bool _prevMaintenanceMode = false;
  AppState _prevAppState = AppState::Startup;
  bool _prevRingingState = false;
  String _prevCallNumber = "";
  String _prevDialingNumber = "";
  String _prevCallStateNumber = ""; // Cache for call state number to avoid string creation
  bool _callWasActive = false;
  unsigned long _callStartTime = 0;

  // Callback for blocked calls
  std::function<void(const String &)> _callBlockedCallback;

  std::vector<std::unique_ptr<IIntegration>> _integrations;

  void registerIntegrations();
};
