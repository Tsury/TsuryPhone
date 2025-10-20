#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION
#include "../common/state.h"
#include "../core/DeviceConfig.h"
#include "core/DeviceStats.h"
#include <functional>
#include <stdint.h>

// Forward declare to avoid circular include
class IntegrationManager;

// Forward declaration
struct IntegrationCallbackResult;

/**
 * Base interface for all device integrations
 * Defines the contract that all integrations must implement
 */
class IIntegration {
public:
  virtual ~IIntegration() = default;

  // Capability bitmask (extend as needed)
  enum IntegrationCapability : uint32_t {
    IC_NONE = 0,
    IC_ACTIONS = 1 << 0,            // Supports generic action triggering
    IC_NUMBER_CODE_HANDLER = 1 << 1 // Provides number code parsing/lookup
  };

  // Lifecycle management
  virtual bool init() = 0;
  virtual void process() = 0;
  virtual void stop() = 0;

  // State synchronization
  virtual void updatePhoneState(AppState newState, AppState previousState) = 0;
  virtual void updateCallInfo(const String &number,
                              bool isIncoming,
                              unsigned long startTime = 0,
                              bool isPriority = false,
                              const String &name = "") = 0;
  virtual void updateDialingProgress(const String &currentNumber) = 0;
  virtual void updateRingState(bool isRinging) = 0;
  virtual void updateSystemStatus() = 0;
  virtual void updateDndState(bool isDndActive) = 0;
  virtual void updateVolumeMode(VolumeMode mode) {
    (void)mode;
  }

  // Device operation callbacks
  virtual void
  setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback) = 0;
  virtual void setDialDigitCallback(std::function<IntegrationCallbackResult(uint8_t)> callback) = 0;
  virtual void setAnswerCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void setHangupCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void
  setRingCallback(std::function<IntegrationCallbackResult(const String &, bool)> callback) = 0;
  virtual void setCallWaitingCallback(std::function<IntegrationCallbackResult()> callback) = 0;
  virtual void
  setVolumeModeCallback(std::function<IntegrationCallbackResult(VolumeMode)> callback) {
    (void)callback;
  }
  virtual void setMaintenanceModeChangedCallback(std::function<void(bool)> callback) = 0;
  virtual void setFactoryResetCallback(std::function<void()> callback) = 0;

  // Statistics and monitoring
  virtual void reportCallStart(const String &number, bool isIncoming) = 0;
  virtual void reportCallEnd(unsigned long duration) = 0;
  virtual void reportBlockedCall(const String &number) = 0;
  virtual void reportError(const String &error) = 0;
  // Generic action triggering entry point (integration interprets actionId in its domain)
  virtual void triggerAction(const String &actionId) = 0;

  // Configuration synchronization
  virtual void onConfigurationChanged() = 0;

  // Integration info
  virtual const char *getName() const = 0;

  // Short tag (<=8 chars) for structured logging; default uses name if not overridden.
  virtual const char *getTag() const {
    return getName();
  }
  // Optional semantic version string for the integration (e.g., "1.0.0").
  // Default returns nullptr meaning version not reported.
  virtual const char *getVersion() const {
    return nullptr;
  }
  virtual bool isEnabled() const = 0;
  // Capability query (default none)
  virtual uint32_t getCapabilities() const {
    return IC_NONE;
  }

  // Allow integrations to register action handlers (default: no handlers)
  virtual void registerActionHandlers(IntegrationManager &) {}
};

#endif // HOME_ASSISTANT_INTEGRATION