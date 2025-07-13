#pragma once

#include "../core/DeviceConfig.h"
#include "../core/DeviceStats.h"
#include "../common/state.h"
#include "IIntegration.h"
#include <functional>
#include <vector>
#include <memory>

/**
 * Manager for all device integrations
 * Uses a collection-based approach to support multiple integrations simultaneously
 * Each integration implements the IIntegration interface
 */
class IntegrationManager {
public:
  IntegrationManager(DeviceConfig& config, DeviceStats& stats);
  ~IntegrationManager();
  
  // Lifecycle management
  bool init();
  void process();
  void stop();
  
  // State synchronization - calls all registered integrations
  void updatePhoneState(AppState newState, AppState previousState);
  void updateCallInfo(const String& number, bool isIncoming, unsigned long startTime = 0);
  void updateDialingProgress(const String& currentNumber);
  void updateRingState(bool isRinging);
  void updateSystemStatus();
  
  // Device operation callbacks - sets callbacks for all integrations
  void setDialCallback(std::function<bool(const String&)> callback);
  void setAnswerCallback(std::function<bool()> callback);
  void setHangupCallback(std::function<bool()> callback);
  void setRingCallback(std::function<bool(const String&)> callback);
  void setWebhookCallback(std::function<bool(const String&)> callback);
  void setCallWaitingCallback(std::function<bool()> callback);
  
  // Statistics and monitoring - reports to all integrations
  void reportCallStart(const String& number, bool isIncoming);
  void reportCallEnd(unsigned long duration);
  void reportBlockedCall(const String& number);
  void reportError(const String& error);
  void reportWebhookTrigger(const String& webhookId);
  
  // Configuration synchronization - notifies all integrations
  void onConfigurationChanged();
  
  // Management
  bool hasEnabledIntegrations() const;
  void listIntegrations() const;

private:
  DeviceConfig& _config;
  DeviceStats& _stats;
  
  std::vector<std::unique_ptr<IIntegration>> _integrations;
  
  void registerIntegrations();
};
