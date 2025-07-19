#pragma once

#include "../common/state.h"
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
  StatsManager(DeviceStats &stats, State &state);

  // Lifecycle
  bool init();
  void process();

  // State change handlers - called automatically by IntegrationManager
  void onPhoneStateChanged(AppState newState, AppState previousState);
  void onCallInfoChanged(const String &number, bool isIncoming);
  void onCallBlocked(const String &number);
  void onDialingProgressChanged(const String &currentNumber);

private:
  DeviceStats &_stats;
  State &_state;

  // State tracking for automatic statistics
  AppState _prevAppState = AppState::Startup;
  bool _callInProgress = false;
  String _currentCallNumber = "";
  bool _currentCallIsIncoming = false;
  unsigned long _callStartTime = 0;
  String _lastDialingNumber = "";
  String _lastProcessedCallNumber = "";

  // Helper methods
  void handleCallStart(const String &number, bool isIncoming);
  void handleCallEnd();
  bool isCallActiveState(AppState state) const;
  bool isDialingState(AppState state) const;
};
