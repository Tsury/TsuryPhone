#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../common/state.h"
#include "../../core/DeviceConfig.h"
#include "DeviceStats.h"
#include <Arduino.h>

/**
 * StatsManager - Automatically tracks call statistics based on state changes
 *
 * This class separates the responsibility of statistics tracking from the main
 * application logic and integrations. It observes state changes and automatically
 * records the appropriate statistics.
 */
class StatsManager {
public:
  StatsManager(DeviceStats &stats, State &state, DeviceConfig &config);

  // Lifecycle
  bool init();
  void process();

  // State change handlers - called automatically by IntegrationManager
  void onPhoneStateChanged(AppState newState, AppState previousState);
  void onCallInfoChanged(const String &number, bool isIncoming, bool isPriority, const String &name);
  void onCallBlocked(const String &number, bool isPriority, const String &name);
  void onDialingProgressChanged(const String &currentNumber);

private:
  DeviceStats &_stats;
  State &_state;
  DeviceConfig &_config;

  // State tracking for automatic statistics
  AppState _prevAppState = AppState::Startup;
  bool _callInProgress = false;
  String _currentCallNumber = "";
  bool _currentCallIsIncoming = false;
  bool _currentCallIsPriority = false;
  String _currentCallName = "";
  unsigned long _callStartTime = 0;
  String _lastDialingNumber = "";
  String _lastProcessedCallNumber = "";

  struct BlockedCallContext {
    String rawNumber;
    String normalizedNumber;
    unsigned long timestampMs = 0;

    bool isActive() const {
      return !rawNumber.isEmpty() || !normalizedNumber.isEmpty();
    }

    void set(const String &raw, const String &normalized, unsigned long timestamp) {
      rawNumber = raw;
      normalizedNumber = normalized;
      timestampMs = timestamp;
    }

    void clear() {
      rawNumber = "";
      normalizedNumber = "";
      timestampMs = 0;
    }

    bool matches(const String &rawCandidate, const String &normalizedCandidate) const {
      if (!isActive()) {
        return false;
      }

      if (!rawCandidate.isEmpty() && !rawNumber.isEmpty() &&
          rawCandidate.equalsIgnoreCase(rawNumber)) {
        return true;
      }

      if (!normalizedCandidate.isEmpty() && !normalizedNumber.isEmpty() &&
          normalizedCandidate.equalsIgnoreCase(normalizedNumber)) {
        return true;
      }

      if (!normalizedCandidate.isEmpty() && !rawNumber.isEmpty() &&
          normalizedCandidate.equalsIgnoreCase(rawNumber)) {
        return true;
      }

      if (!rawCandidate.isEmpty() && !normalizedNumber.isEmpty() &&
          rawCandidate.equalsIgnoreCase(normalizedNumber)) {
        return true;
      }

      return false;
    }
  };

  BlockedCallContext _pendingBlockedCall;

  // Helper methods
  void handleCallStart(const String &number, bool isIncoming, bool isPriority, const String &name);
  void handleCallEnd();
  bool isCallActiveState(AppState state) const;
  bool isDialingState(AppState state) const;
  bool isIncomingAlertState(AppState state) const;
  void handleMissedIncomingCall(const String &number, const String &name, bool isPriority);
  void handleUnansweredOutgoingCall(const String &number, const String &name, bool isPriority);
  String resolveLastKnownNumber() const;
  String resolveCallerName(const String &number) const;
};

#endif // HOME_ASSISTANT_INTEGRATION