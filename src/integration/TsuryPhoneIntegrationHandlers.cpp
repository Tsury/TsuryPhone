#include "main.h"
#include "common/logger.h"

#ifdef HOME_ASSISTANT_INTEGRATION

// Keep only integration-specific TsuryPhone method implementations here to declutter main.cpp

IntegrationCallbackResult TsuryPhone::handleIntegrationDialRequest(const String &number) {
  Logger::infoln(F("Integration dial request: %s"), number.c_str());

  if (_state.newAppState == AppState::Idle && _hookSwitch.isOffHook()) {
    NumberValidationResult validation = _numberHandler.validateNumber(number.c_str());
    if (validation.isComplete && (validation.action == NumberAction::QuickDial ||
                                  validation.action == NumberAction::DirectDial)) {
      _modem.enqueueCall(validation.targetNumber.c_str());
      return IntegrationCallbackResult(true);
    } else {
      String error = "Invalid number: " + number;
      Logger::errorln(F("Integration dial request: %s"), error.c_str());
      return IntegrationCallbackResult(false, error);
    }
  } else {
    String error = "Phone not ready (state: " + String(appStateToString(_state.newAppState)) +
                   ", hook: " + String(_hookSwitch.isOffHook() ? "off" : "on") + ")";
    Logger::errorln(F("Integration dial request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error);
  }
}

IntegrationCallbackResult TsuryPhone::handleIntegrationAnswerRequest() {
  Logger::infoln(F("Integration answer request"));
  if (_state.newAppState == AppState::IncomingCall ||
      _state.newAppState == AppState::IncomingCallRing) {
    _modem.answer();
    return IntegrationCallbackResult(true);
  }
  String error = "No incoming call (state: " + String(appStateToString(_state.newAppState)) + ")";
  Logger::errorln(F("Integration answer request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationHangupRequest() {
  Logger::infoln(F("Integration hangup request"));
  if (_state.newAppState == AppState::InCall || _state.newAppState == AppState::Dialing ||
      _state.newAppState == AppState::IncomingCall ||
      _state.newAppState == AppState::IncomingCallRing) {
    _modem.hangUp();
    return IntegrationCallbackResult(true);
  }
  String error = "No active call (state: " + String(appStateToString(_state.newAppState)) + ")";
  Logger::errorln(F("Integration hangup request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationRingRequest(const String &pattern) {
  Logger::infoln(F("Integration ring request: %s"), pattern.c_str());
  if (_state.newAppState == AppState::Idle) {
    if (pattern.isEmpty()) {
      String configPattern = _deviceConfig.getRingPattern();
      _ringer.startRinging(configPattern);
    } else {
      _ringer.startRinging(pattern);
    }
    return IntegrationCallbackResult(true);
  }
  String error = "Phone not idle (state: " + String(appStateToString(_state.newAppState)) + ")";
  Logger::errorln(F("Integration ring request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationCallWaitingRequest() {
  Logger::infoln(F("Integration call waiting request"));
  if (_state.newAppState == AppState::InCall && _state.callState.hasCallWaiting()) {
    _modem.switchToCallWaiting();
    return IntegrationCallbackResult(true);
  }
  String error = "No active call with call waiting (state: " + String(appStateToString(_state.newAppState)) +
                 ", has waiting: " + String(_state.callState.hasCallWaiting() ? "yes" : "no") + ")";
  Logger::errorln(F("Integration call waiting request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

void TsuryPhone::handleIntegrationCallBlocked(const String &number) {
  Logger::infoln(F("Integration call blocked: %s"), number.c_str());
  _modem.hangUp();
  setState(AppState::Idle);
}

void TsuryPhone::onAudioConfigChanged() {
  Logger::infoln(F("Audio configuration changed, updating modem settings"));
  if (_modem.getCurrentVolumeMode() == VolumeMode::Earpiece) {
    _modem.setEarpieceVolume();
  } else {
    _modem.setSpeakerVolume();
  }
}

void TsuryPhone::handleIntegrationMaintenanceModeChanged(const bool enabled) {
  Logger::infoln(F("Integration maintenance mode change request: %s"),
                 enabled ? "enabled" : "disabled");
  onMaintenanceModeChanged(enabled);
}

void TsuryPhone::handleIntegrationConfigChanged(ConfigChangeEvent event) {
  Logger::infoln(F("Integration config change event"));
  switch (event) {
  case ConfigChangeEvent::DND_CONFIG_CHANGED:
    Logger::infoln(F("DND config changed, updating DND state"));
    _timeManager.determineDndState(_state);
    break;
  case ConfigChangeEvent::AUDIO_CONFIG_CHANGED:
    Logger::infoln(F("Audio config changed"));
    onAudioConfigChanged();
    break;
  default:
    break;
  }
}

#endif // HOME_ASSISTANT_INTEGRATION
