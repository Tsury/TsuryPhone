#pragma once

#include "../common/state.h"
#include "../core/DeviceConfig.h"
#include "../core/DeviceStats.h"
#include <functional>

// Forward declaration
struct IntegrationCallbackResult;

/**
 * Base interface for all device integrations
 * Defines the contract that all integrations must implement
 */
class IIntegration {
public:
  virtual ~IIntegration() = default;

  // Lifecycle management
  virtual bool init() = 0;
  virtual void process() = 0;
  virtual void stop() = 0;

  // State synchronization
  virtual void updatePhoneState(AppState newState, AppState previousState) = 0;
  virtual void
  updateCallInfo(const String &number, bool isIncoming, unsigned long startTime = 0) = 0;
  virtual void updateDialingProgress(const String &currentNumber) = 0;
  virtual void updateRingState(bool isRinging) = 0;
  virtual void updateSystemStatus() = 0;
  virtual void updateDndState(bool isDndActive) = 0;

  // Device operation callbacks
  virtual void setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) = 0;
  virtual void setAnswerCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void setHangupCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void setRingCallback(std::function<IntegrationCallbackResult(const String &)> callback) = 0;
  virtual void setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void setMaintenanceModeChangedCallback(std::function<void(bool)> callback) = 0;

  // Statistics and monitoring
  virtual void reportCallStart(const String &number, bool isIncoming) = 0;
  virtual void reportCallEnd(unsigned long duration) = 0;
  virtual void reportBlockedCall(const String &number) = 0;
  virtual void reportError(const String &error) = 0;
  virtual void triggerWebhook(const String &webhookId) = 0;

  // Configuration synchronization
  virtual void onConfigurationChanged() = 0;

  // Integration info
  virtual const char *getName() const = 0;
  virtual bool isEnabled() const = 0;
};
