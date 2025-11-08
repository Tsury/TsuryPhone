#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION
#include "../common/state.h"
#include "../core/DeviceConfig.h"
#include "IntegrationTypes.h"
#include "core/DeviceStats.h"
#include <ArduinoJson.h>
#include <functional>

// Central event schema version constant (T6.2 prework)
#ifndef INTEGRATION_EVENT_SCHEMA_VERSION
#define INTEGRATION_EVENT_SCHEMA_VERSION 3
#endif
static_assert(INTEGRATION_EVENT_SCHEMA_VERSION == 3, "Unexpected schema version change");

/**
 * Contains all the common business logic for device integrations
 * This class is protocol-agnostic and handles the core phone operations
 */
class IntegrationService {
public:
  IntegrationService(DeviceConfig &config, DeviceStats &stats, State &state);
  // N1: Provide access to a shared singleton instance to reduce memory when multiple
  // integrations are active. The first call initializes the static instance.
  static IntegrationService &shared(DeviceConfig &config, DeviceStats &stats, State &state);

  // Device operation callbacks (set by main application)
  void setDialCallback(std::function<IntegrationCallbackResult(const String &)> callback);
  void setDialDigitCallback(std::function<IntegrationCallbackResult(char, bool)> callback);
  void setSendDTMFCallback(std::function<IntegrationCallbackResult(char)> callback);
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

  // Core business logic methods - these contain the actual logic
  IntegrationCallbackResult handleDialRequest(const String &number);
  IntegrationCallbackResult handleDialDigit(char digit, bool deferValidation = false);
  IntegrationCallbackResult handleSendDTMF(char digit);
  IntegrationCallbackResult handleDeleteLastDigit();
  IntegrationCallbackResult handleSendDialedNumber();
  IntegrationCallbackResult handleAnswerRequest();
  IntegrationCallbackResult handleHangupRequest();
  IntegrationCallbackResult handleDialQuickDial(const String &code);
  IntegrationCallbackResult handleToggleCallWaiting();
  IntegrationCallbackResult handleRingOperation(const String &pattern, bool force = false);
  IntegrationCallbackResult handleSetVolumeMode(VolumeMode mode);
  IntegrationCallbackResult handleToggleVolumeMode();
  IntegrationCallbackResult handleToggleMute();

  // Configuration management
  IntegrationCallbackResult handleSetDND(const JsonVariant &json);
  IntegrationCallbackResult handleSetMaintenanceMode(bool enabled);
  IntegrationCallbackResult handleSetAudioConfig(const JsonVariant &json);
  IntegrationCallbackResult handleSetRingPattern(const String &pattern);
  IntegrationCallbackResult handleSetDialingConfig(const JsonVariant &json);

  // Quick dial management
  IntegrationCallbackResult
  handleAddQuickDial(const String &code, const String &number, const String &name = "");
  IntegrationCallbackResult handleRemoveQuickDialById(const String &id);
  IntegrationCallbackResult handleEditContact(const String &id,
                                               const String &name,
                                               const String &number,
                                               const String &code,
                                               bool isPriority);

  // Blocked numbers management
  IntegrationCallbackResult handleAddBlockedNumber(const String &number, const String &name = "");
  IntegrationCallbackResult handleRemoveBlockedNumberById(const String &id);

  // Priority callers management
  IntegrationCallbackResult handleAddPriorityCaller(const String &number);
  IntegrationCallbackResult handleRemovePriorityCallerById(const String &id);

  // Data operations
  IntegrationCallbackResult handleGetTsuryPhoneConfig();
  IntegrationCallbackResult handleRefetchAll();

  // Device control
  IntegrationCallbackResult handleResetDevice();
  IntegrationCallbackResult handleFactoryReset();
  bool processScheduledReset();

  // All legacy webhook-specific triggering removed from generic layer; integrations map
  // external transports/protocols to these neutral operations.

  // JSON serialization methods for integration clients
  JsonDocument buildAllData();
  JsonDocument buildSystemStatus();
  JsonDocument buildPhoneConfig();
  JsonDocument buildCallStats();
  JsonDocument buildPhoneData();
  JsonDocument buildDeviceConfig();

  // Individual JSON builders (for composing larger responses)
  void addStatus(JsonObject &doc);
  void addConfig(JsonObject &doc);
  void addStats(JsonObject &doc);
  void addPhone(JsonObject &doc);

  // Data builders for events and broadcasts (integration-agnostic)
  void addBasicDeviceInfo(JsonObject &obj);
  void addPhoneStateInfo(JsonObject &obj);
  void addBasicPhoneStatus(JsonObject &obj); // For status endpoint - no extra fields
  void addCallInfo(JsonObject &obj,
                   const String &callNumber = "",
                   bool isIncoming = false,
                   unsigned long startTime = 0);
  void addSystemInfo(JsonObject &obj);
  void addStatsInfo(JsonObject &obj);
  // Create a new event object using v2 root fields (category,event,seq,...)
  JsonObject createEventObject(JsonDocument &doc, const String &category, const String &event);
  // Integration tag setter (e.g., "ha") used in root event field
  void setIntegrationTag(const char *tag) {
    _integrationTag = tag ? tag : "core";
  }
  void getFullStatus(JsonObject &obj);

  // Event-specific builders
  JsonDocument buildShutdownEvent(const String &reason = "reset_requested");
  JsonDocument buildErrorEvent(const String &error);
  JsonDocument buildCallEvent(const String &eventType,
                              const String &number = "",
                              bool isIncoming = false,
                              unsigned long duration = 0);
  JsonDocument buildPhoneStateEvent(const String &eventType,
                                    AppState newState = AppState::Idle,
                                    AppState previousState = AppState::Idle,
                                    const String &currentNumber = "");
  JsonDocument buildSystemEvent(const String &eventType);
  JsonDocument buildFullStateEvent(const String &callNumber = "",
                                   bool isIncoming = false,
                                   unsigned long startTime = 0);

  // I1: Lightweight change detector for stats without building JSON
  uint32_t getStatsFingerprint() const;

  // Convenience methods that use current state - these don't require state tracking by integrations
  JsonDocument buildCurrentPhoneStateEvent(const String &eventType);
  JsonDocument buildCurrentCallEvent(const String &eventType, unsigned long duration = 0);
  JsonDocument buildCurrentFullStateEvent();
  // Build config_delta event (single key change) using canonical {key,oldValue?,newValue}
  JsonDocument buildSystemConfigEvent(const String &key,
                                      const JsonVariant &newValue,
                                      const JsonVariantConst &oldValue = JsonVariantConst());
  // Build aggregated config_delta event for multiple changes (obj should contain base root fields
  // only before calling)
  JsonDocument buildAggregatedConfigDeltaEvent(
      const std::vector<std::tuple<String, JsonVariantConst, JsonVariantConst>> &changes);

  // DS2: call timing helpers
  void setCurrentCallStartTs(unsigned long ts) {
    _currentCallStartTs = ts;
  }
  unsigned long getCurrentCallStartTs() const {
    return _currentCallStartTs;
  }
  void clearCurrentCallStartTs() {
    _currentCallStartTs = 0;
  }
  void setCurrentCallDirection(bool incoming) {
    _currentCallIsIncoming = incoming;
  }
  bool getCurrentCallIsIncoming() const {
    return _currentCallIsIncoming;
  }

  // DS4: Unified HTTP response helpers (generic for all integrations)
  JsonDocument buildSuccessResponse(const JsonVariantConst &data = JsonVariantConst());
  JsonDocument buildErrorResponse(const String &message);
  JsonDocument buildErrorResponse(const String &message, const String &errorCode);

private:
  DeviceConfig &_config;
  DeviceStats &_stats;
  State &_state;

  String resolveCallerName(const String &number) const;

  // Device operation callbacks
  std::function<IntegrationCallbackResult(const String &)> _dialCallback;
  std::function<IntegrationCallbackResult(char, bool)> _dialDigitCallback;
  std::function<IntegrationCallbackResult(char)> _sendDTMFCallback;
  std::function<IntegrationCallbackResult()> _deleteLastDigitCallback;
  std::function<IntegrationCallbackResult()> _sendDialedNumberCallback;
  std::function<IntegrationCallbackResult()> _answerCallback;
  std::function<IntegrationCallbackResult()> _hangupCallback;
  std::function<IntegrationCallbackResult(const String &, bool)> _ringCallback;
  std::function<IntegrationCallbackResult()> _callWaitingCallback;
  std::function<IntegrationCallbackResult(VolumeMode)> _volumeModeCallback;
  std::function<IntegrationCallbackResult()> _toggleMuteCallback;
  std::function<void(bool)> _maintenanceModeChangedCallback;
  std::function<void()> _factoryResetCallback;
  // Removed per C2: config change events now routed exclusively via IntegrationManager

  // Reset scheduling
  bool _resetRequested = false;
  bool _factoryResetRequested = false;
  unsigned long _resetScheduledTime = 0;

  // Helper methods (none currently for generic actions)

public:
  // Reset scheduling access for integrations
  bool isResetRequested() const {
    return _resetRequested;
  }
  unsigned long getResetScheduledTime() const {
    return _resetScheduledTime;
  }
  void clearResetRequest() {
    _resetRequested = false;
  }

private:
  CallRecord buildCurrentCallSnapshot(const CallRecord &base,
                                      const String &numberHint,
                                      bool incomingHint,
                                      bool priorityHint) const;
  LastCallRecord buildLastCallSnapshot(const LastCallRecord &base) const;
  void serializeCallRecord(JsonObject &target,
                           const CallRecord &record,
                           const char *presenceKey,
                           unsigned long startTs,
                           uint32_t durationOverride,
                           bool includeNormalized) const;
  void serializeLastCallRecord(JsonObject &target,
                               const LastCallRecord &record,
                               bool includeNormalized) const;

  const char *_integrationTag = "core"; // included in every event root
  uint32_t _eventSeq = 0;               // monotonically increasing sequence
  uint32_t nextSeq() {
    return ++_eventSeq;
  }
  unsigned long _currentCallStartTs = 0; // ms since boot for active call
  bool _currentCallIsIncoming = false;   // DS3: direction persistence across lifecycle
};

#endif // HOME_ASSISTANT_INTEGRATION