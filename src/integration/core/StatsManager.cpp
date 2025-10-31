#ifdef HOME_ASSISTANT_INTEGRATION

#include "StatsManager.h"
#include "../../common/logger.h"
#include "../IntegrationLookup.h"

namespace {
  constexpr const char *kResultAnswered = "answered";
  constexpr unsigned long kBlockedContextTtlMs = 60000UL;
}

StatsManager::StatsManager(DeviceStats &stats, State &state, DeviceConfig &config)
    : _stats(stats),
      _state(state),
      _config(config),
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
  if (strcmp(_state.callState.active.number, _lastProcessedCallNumber.c_str()) != 0 &&
      _state.callState.active.number[0] != '\0') {
    _lastProcessedCallNumber = String(_state.callState.active.number);

    // Determine if it's incoming or outgoing based on state
    bool isIncoming = (_state.newAppState == AppState::IncomingCall ||
                       _state.newAppState == AppState::IncomingCallRing);
    bool isPriority = _state.callState.active.isPriority;
    String callerName = resolveCallerName(_lastProcessedCallNumber);
    onCallInfoChanged(_lastProcessedCallNumber, isIncoming, isPriority, callerName);
  }
}

void StatsManager::onPhoneStateChanged(AppState newState, AppState previousState) {
  // Guard against duplicate notifications
  static AppState lastLoggedPrevious = AppState::Startup;
  static AppState lastLoggedNew = AppState::Startup;

  if (lastLoggedPrevious == previousState && lastLoggedNew == newState) {
    Logger::debugln(F("StatsManager: Duplicate state change ignored: %d to %d"),
                    static_cast<int>(previousState),
                    static_cast<int>(newState));
    return;
  }

  lastLoggedPrevious = previousState;
  lastLoggedNew = newState;

  Logger::infoln(F("StatsManager: State changed from %d to %d"),
                 static_cast<int>(previousState),
                 static_cast<int>(newState));

  bool wasInCall = isCallActiveState(previousState);
  bool isInCall = isCallActiveState(newState);

  // Handle call start transitions (only when call is fully in progress)
  if (!wasInCall && isInCall) {
    String number = resolveLastKnownNumber();
    bool isIncoming = _currentCallIsIncoming;
    if (previousState == AppState::IncomingCall || previousState == AppState::IncomingCallRing) {
      isIncoming = true;
    } else if (previousState == AppState::Dialing) {
      isIncoming = false;
    }
    bool isPriority = _state.callState.active.isPriority;
    String callerName = resolveCallerName(number);
    handleCallStart(number, isIncoming, isPriority, callerName);
  }

  // Handle call end transitions once a call was established
  if (wasInCall && !isInCall && _callInProgress) {
    handleCallEnd();
  }

  bool wasIncomingAlert = isIncomingAlertState(previousState);
  bool isIncomingAlert = isIncomingAlertState(newState);

  // Detect incoming calls that stopped ringing without being answered
  if (wasIncomingAlert && !isIncomingAlert && !isInCall && !_callInProgress) {
    String missedNumber = resolveLastKnownNumber();
    String callerName = resolveCallerName(missedNumber);
    bool isPriority = _state.callState.active.isPriority;
    handleMissedIncomingCall(missedNumber, callerName, isPriority);
  }

  // Capture dialing number when entering dialing state
  if (newState == AppState::Dialing && previousState != AppState::Dialing) {
    String currentNumber = String(_state.callState.active.number);
    if (!currentNumber.isEmpty()) {
      _lastDialingNumber = currentNumber;
      Logger::debugln(F("StatsManager: Captured dialing number: %s"), currentNumber.c_str());
    }
  }

  // Detect outgoing attempts that never connected (dialing -> idle without active call)
  if (previousState == AppState::Dialing && newState == AppState::Idle && !_callInProgress) {
    String dialingNumber =
        _lastDialingNumber.isEmpty() ? resolveLastKnownNumber() : _lastDialingNumber;
    String callerName = resolveCallerName(dialingNumber);
    handleUnansweredOutgoingCall(dialingNumber, callerName, false);
  }
}

void StatsManager::onCallInfoChanged(const String &number,
                                     bool isIncoming,
                                     bool isPriority,
                                     const String &name) {
  if (_pendingBlockedCall.isActive()) {
    _pendingBlockedCall.clear();
  }

  String resolvedName = name.isEmpty() ? resolveCallerName(number) : name;
  Logger::debugln(F("StatsManager: Call info changed - dir=%s number=%s priority=%s name=%s"),
                  isIncoming ? F("incoming") : F("outgoing"),
                  number.c_str(),
                  isPriority ? F("true") : F("false"),
                  resolvedName.c_str());

  _currentCallNumber = number;
  _currentCallIsIncoming = isIncoming;
  _currentCallIsPriority = isPriority;
  _currentCallName = resolvedName;

  // If we're already in a call state, start tracking immediately
  if (isCallActiveState(_state.newAppState) && !_callInProgress) {
    handleCallStart(number, isIncoming, isPriority, _currentCallName);
  }
  // If call is already in progress, update the cached CallRecord for call waiting leg swaps
  else if (_callInProgress) {
    Logger::infoln(F("StatsManager: Updating current call record during active call (leg swap)"));
    _stats.updateCurrentCall(number, resolvedName, isIncoming, isPriority);
  }
}

void StatsManager::onCallBlocked(const String &number, bool isPriority, const String &name) {
  String callerName = name.isEmpty() ? resolveCallerName(number) : name;
  Logger::infoln(F("StatsManager: Call blocked from %s"), number.c_str());
  _stats.recordBlockedCall(number, callerName, isPriority);
  _stats.clearCurrentCall();
  _callInProgress = false;
  _currentCallNumber = "";
  _currentCallIsIncoming = false;
  _currentCallIsPriority = false;
  _currentCallName = "";
  _callStartTime = 0;
  _lastProcessedCallNumber = "";
  _lastDialingNumber = "";

  String normalizedBlocked = _config.normalizeNumber(number);
  _pendingBlockedCall.set(number, normalizedBlocked, millis());
}

void StatsManager::onDialingProgressChanged(const String &currentNumber) {
  Logger::infoln(F("StatsManager: Dialing progress changed to %s"), currentNumber.c_str());
  _lastDialingNumber = currentNumber;
}

void StatsManager::handleCallStart(const String &number,
                                   bool isIncoming,
                                   bool isPriority,
                                   const String &name) {
  if (_pendingBlockedCall.isActive()) {
    _pendingBlockedCall.clear();
  }

  if (_callInProgress) {
    Logger::warnln(F("StatsManager: Call already in progress, ignoring start"));
    return;
  }

  String resolvedNumber = number;
  if (resolvedNumber.isEmpty()) {
    resolvedNumber = resolveLastKnownNumber();
  }

  String resolvedName = name;
  if (resolvedName.isEmpty()) {
    resolvedName = resolveCallerName(resolvedNumber);
  }

  Logger::infoln(F("StatsManager: Recording call start - dir=%s number=%s priority=%s name=%s"),
                 isIncoming ? F("incoming") : F("outgoing"),
                 resolvedNumber.isEmpty() ? "<unknown>" : resolvedNumber.c_str(),
                 isPriority ? F("true") : F("false"),
                 resolvedName.c_str());

  _stats.beginCall(resolvedNumber, resolvedName, isIncoming, isPriority);

  _callInProgress = true;
  _callStartTime = millis();
  _currentCallNumber = resolvedNumber;
  _currentCallIsIncoming = isIncoming;
  _currentCallIsPriority = isPriority;
  _currentCallName = resolvedName;
}

void StatsManager::handleCallEnd() {
  if (!_callInProgress) {
    Logger::warnln(F("StatsManager: No call in progress, ignoring end"));
    return;
  }

  Logger::infoln(F("StatsManager: Recording call end"));

  _stats.finalizeCurrentCall(String(kResultAnswered));

  // Reset call tracking
  _callInProgress = false;
  _currentCallNumber = "";
  _currentCallIsIncoming = false;
  _currentCallIsPriority = false;
  _currentCallName = "";
  _callStartTime = 0;
  _lastProcessedCallNumber = "";
  _lastDialingNumber = "";

  if (_pendingBlockedCall.isActive()) {
    _pendingBlockedCall.clear();
  }
}

bool StatsManager::isCallActiveState(AppState state) const {
  return state == AppState::InCall;
}

bool StatsManager::isDialingState(AppState state) const {
  return state == AppState::Dialing;
}

bool StatsManager::isIncomingAlertState(AppState state) const {
  return state == AppState::IncomingCall || state == AppState::IncomingCallRing;
}

void StatsManager::handleMissedIncomingCall(const String &number,
                                            const String &name,
                                            bool isPriority) {
  String resolvedNumber = number;
  if (resolvedNumber.isEmpty()) {
    resolvedNumber = resolveLastKnownNumber();
  }

  String resolvedName = name;
  if (resolvedName.isEmpty()) {
    resolvedName = resolveCallerName(resolvedNumber);
  }

  if (resolvedNumber.isEmpty()) {
    Logger::debugln(F("StatsManager: Missed incoming call with unknown number"));
    return;
  }

  const unsigned long now = millis();
  if (_pendingBlockedCall.isActive()) {
    String normalizedCandidate = _config.normalizeNumber(resolvedNumber);
    if (_pendingBlockedCall.matches(resolvedNumber, normalizedCandidate)) {
      Logger::infoln(F("StatsManager: Skipping missed-call record; blocked number %s"),
                     resolvedNumber.c_str());
      _pendingBlockedCall.clear();
      return;
    }

    if (_pendingBlockedCall.timestampMs > 0 &&
        (now - _pendingBlockedCall.timestampMs) > kBlockedContextTtlMs) {
      _pendingBlockedCall.clear();
    }
  }

  Logger::infoln(F("StatsManager: Recording missed incoming call from %s"), resolvedNumber.c_str());

  _stats.recordMissedIncomingCall(resolvedNumber, resolvedName, isPriority);
  _stats.clearCurrentCall();
  _currentCallNumber = "";
  _lastProcessedCallNumber = "";
  _lastDialingNumber = "";
  _currentCallIsIncoming = false;
  _currentCallIsPriority = false;
  _currentCallName = "";
}

void StatsManager::handleUnansweredOutgoingCall(const String &number,
                                                const String &name,
                                                bool isPriority) {
  String resolvedNumber = number;

  Logger::debugln(F("StatsManager: Resolving number for unanswered call - current: '%s', "
                    "processed: '%s', state: '%s', dialing: '%s'"),
                  _currentCallNumber.c_str(),
                  _lastProcessedCallNumber.c_str(),
                  _state.callState.active.number,
                  _lastDialingNumber.c_str());

  if (resolvedNumber.isEmpty()) {
    resolvedNumber = resolveLastKnownNumber();
  }

  String resolvedName = name;
  if (resolvedName.isEmpty()) {
    resolvedName = resolveCallerName(resolvedNumber);
  }

  if (resolvedNumber.isEmpty()) {
    Logger::warnln(F("StatsManager: Outgoing call attempt without number"));
    return;
  }

  Logger::infoln(F("StatsManager: Recording unanswered outgoing call to %s"),
                 resolvedNumber.c_str());
  _stats.recordUnansweredOutgoingCall(resolvedNumber, resolvedName, isPriority);
  _stats.clearCurrentCall();
  _currentCallNumber = "";
  _lastProcessedCallNumber = "";
  _lastDialingNumber = "";
  _currentCallIsIncoming = false;
  _currentCallIsPriority = false;
  _currentCallName = "";
}

String StatsManager::resolveLastKnownNumber() const {
  if (!_currentCallNumber.isEmpty()) {
    return _currentCallNumber;
  }

  if (!_lastProcessedCallNumber.isEmpty()) {
    return _lastProcessedCallNumber;
  }

  if (_state.callState.active.number[0] != '\0') {
    return String(_state.callState.active.number);
  }

  if (!_lastDialingNumber.isEmpty()) {
    return _lastDialingNumber;
  }

  return String();
}

String StatsManager::resolveCallerName(const String &number) const {
  if (number.isEmpty()) {
    return String();
  }

  if (!_currentCallName.isEmpty() && number == _currentCallNumber) {
    return _currentCallName;
  }

  return IntegrationLookup::lookupCallerName(_config, number);
}

#endif // HOME_ASSISTANT_INTEGRATION