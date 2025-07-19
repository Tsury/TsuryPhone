#include "StatsManager.h"
#include "../common/logger.h"

StatsManager::StatsManager(DeviceStats &stats, State &state)
    : _stats(stats),
      _state(state),
      _prevAppState(AppState::Startup),
      _callInProgress(false),
      _callStartTime(0) {}

bool StatsManager::init() {
  Logger::infoln(F("Initializing StatsManager..."));

  // Initialize state tracking
  _prevAppState = _state.newAppState;
  _callInProgress = isCallActiveState(_state.newAppState);

  Logger::infoln(F("StatsManager initialized successfully"));
  return true;
}

void StatsManager::process() {
  // Check for state changes and update statistics accordingly
  if (_state.newAppState != _prevAppState) {
    onPhoneStateChanged(_state.newAppState, _prevAppState);
    _prevAppState = _state.newAppState;
  }

  // Check for call number changes (for automatic call info updates)
  if (strcmp(_state.callState.callNumber, _lastProcessedCallNumber.c_str()) != 0 &&
      _state.callState.callNumber[0] != '\0') {
    _lastProcessedCallNumber = String(_state.callState.callNumber);

    // Determine if it's incoming or outgoing based on state
    bool isIncoming = (_state.newAppState == AppState::IncomingCall ||
                       _state.newAppState == AppState::IncomingCallRing);
    onCallInfoChanged(_lastProcessedCallNumber, isIncoming);
  }
}

void StatsManager::onPhoneStateChanged(AppState newState, AppState previousState) {
  Logger::infoln(F("StatsManager: State changed from %d to %d"),
                 static_cast<int>(previousState),
                 static_cast<int>(newState));

  // Handle call start transitions
  if (!isCallActiveState(previousState) && isCallActiveState(newState)) {
    // Call is starting
    if (!_currentCallNumber.isEmpty()) {
      handleCallStart(_currentCallNumber, _currentCallIsIncoming);
    }
  }

  // Handle call end transitions
  if (isCallActiveState(previousState) && !isCallActiveState(newState)) {
    // Call is ending
    if (_callInProgress) {
      handleCallEnd();
    }
  }
}

void StatsManager::onCallInfoChanged(const String &number, bool isIncoming) {
  Logger::infoln(F("StatsManager: Call info changed - %s call to/from %s"),
                 isIncoming ? F("Incoming") : F("Outgoing"),
                 number.c_str());

  _currentCallNumber = number;
  _currentCallIsIncoming = isIncoming;

  // If we're already in a call state, start tracking immediately
  if (isCallActiveState(_state.newAppState) && !_callInProgress) {
    handleCallStart(number, isIncoming);
  }
}

void StatsManager::onCallBlocked(const String &number) {
  Logger::infoln(F("StatsManager: Call blocked from %s"), number.c_str());
  _stats.recordBlockedCall(number);
}

void StatsManager::onDialingProgressChanged(const String &currentNumber) {
  Logger::infoln(F("StatsManager: Dialing progress changed to %s"), currentNumber.c_str());
  _lastDialingNumber = currentNumber;
  // Note: Dialing progress is mainly for integration notifications,
  // actual call stats are handled by state changes
}

void StatsManager::handleCallStart(const String &number, bool isIncoming) {
  if (_callInProgress) {
    Logger::warnln(F("StatsManager: Call already in progress, ignoring start"));
    return;
  }

  Logger::infoln(F("StatsManager: Recording call start - %s call to/from %s"),
                 isIncoming ? F("Incoming") : F("Outgoing"),
                 number.c_str());

  // Record the appropriate call type
  if (isIncoming) {
    _stats.recordIncomingCall(number);
  } else {
    _stats.recordOutgoingCall(number);
  }

  // Record call start timing
  _stats.recordCallStart();

  _callInProgress = true;
  _callStartTime = millis();
}

void StatsManager::handleCallEnd() {
  if (!_callInProgress) {
    Logger::warnln(F("StatsManager: No call in progress, ignoring end"));
    return;
  }

  Logger::infoln(F("StatsManager: Recording call end"));

  // Record call end
  _stats.recordCallEnd();

  // Reset call tracking
  _callInProgress = false;
  _currentCallNumber = "";
  _currentCallIsIncoming = false;
  _callStartTime = 0;
}

bool StatsManager::isCallActiveState(AppState state) const {
  return state == AppState::InCall || state == AppState::IncomingCallRing;
}

bool StatsManager::isDialingState(AppState state) const {
  return state == AppState::Dialing;
}
