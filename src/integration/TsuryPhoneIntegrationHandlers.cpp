#ifdef HOME_ASSISTANT_INTEGRATION

// clang-format off
// main.h must precede logger to avoid HTTP_* enum clashes
#include "main.h"
#include "common/logger.h"
#include "common/string.h"
// clang-format on

namespace {
  constexpr int kVolumeToggleToneDurationMs = 75;
  constexpr int kResetToneDuration = 500;
  constexpr int kInvalidNumberMp3RepeatCount = 100;
}

// Keep only integration-specific TsuryPhone method implementations here to declutter main.cpp

IntegrationCallbackResult TsuryPhone::handleIntegrationDialRequest(const String &number) {
  Logger::infoln(F("Integration dial request: %s"), number.c_str());

  if (_state.newAppState == AppState::Idle) {
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
  }

  String error = "Phone not idle (state: " + String(appStateToString(_state.newAppState)) + ")";
  Logger::errorln(F("Integration dial request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationDialDigitRequest(uint8_t digit,
                                                                        bool deferValidation) {
  Logger::infoln(F("Integration dial digit request: %u (defer: %s)"),
                 static_cast<unsigned>(digit),
                 deferValidation ? "yes" : "no");

  if (digit > 9) {
    String error = "Digit must be between 0 and 9";
    Logger::errorln(F("Integration dial digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_INVALID_DIGIT");
  }

  if (_state.newAppState != AppState::Idle) {
    String error = "Phone not idle (state: " + String(appStateToString(_state.newAppState)) + ")";
    Logger::errorln(F("Integration dial digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "PHONE_NOT_READY");
  }

  size_t len = strlen(_state.currentDialingNumber);
  if (len >= sizeof(_state.currentDialingNumber) - 1) {
    String error = "Dial buffer full";
    Logger::errorln(F("Integration dial digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_DIAL_BUFFER_FULL");
  }

  // Use the main handleDialedDigitInput function with skipValidation flag
  if (!handleDialedDigitInput(digit, true, true, deferValidation)) {
    String error = "Failed to process digit";
    Logger::errorln(F("Integration dial digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_INVALID_DIGIT");
  }

  return IntegrationCallbackResult(true);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationDeleteLastDigitRequest() {
  Logger::infoln(F("Integration delete last digit request"));

  // Check if phone is in valid state for deleting digits
  if (_state.newAppState != AppState::Idle) {
    String error = "Phone must be idle to delete digits";
    Logger::errorln(F("Integration delete last digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_INVALID_STATE");
  }

  // Check if there are any digits to delete
  size_t len = strlen(_state.currentDialingNumber);
  if (len == 0) {
    String error = "No digits to delete";
    Logger::errorln(F("Integration delete last digit request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_EMPTY_BUFFER");
  }

  // Delete the last digit
  _state.currentDialingNumber[len - 1] = '\0';
  
  Logger::infoln(F("Deleted last digit. Remaining: %s"), _state.currentDialingNumber);

  // Notify integration of the updated dialing progress
  if (_integrationManager) {
    _integrationManager->updateDialingProgress(_state.currentDialingNumber);
  }

  return IntegrationCallbackResult(true, "Last digit deleted");
}

IntegrationCallbackResult TsuryPhone::handleIntegrationSendDialedNumberRequest() {
  Logger::infoln(F("Integration send dialed number request"));

  if (_state.newAppState != AppState::Idle) {
    String error = "Phone not idle (state: " + String(appStateToString(_state.newAppState)) + ")";
    Logger::errorln(F("Integration send dialed number request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "PHONE_NOT_READY");
  }

  if (_state.currentDialingNumber[0] == '\0') {
    String error = "No digits to send";
    Logger::errorln(F("Integration send dialed number request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "WEB_NO_DIGITS");
  }

  Logger::infoln(F("Sending dialed number: %s"), _state.currentDialingNumber);

  const String dialedString(_state.currentDialingNumber);

  // Check for action codes first
  if (_integrationManager && _integrationManager->isActionCode(dialedString)) {
    const String actionId = _integrationManager->resolveActionId(dialedString);
    if (!actionId.isEmpty()) {
      Logger::infoln(F("Action trigger %s"), actionId.c_str());
      _integrationManager->triggerAction(actionId);
      _state.currentDialingNumber[0] = '\0';
      if (_integrationManager) {
        _integrationManager->updateDialingProgress("");
      }
      return IntegrationCallbackResult(true);
    }
  }

  // Validate the number
  const NumberValidationResult numberValidation =
      _numberHandler.validateNumber(_state.currentDialingNumber);

  if (!numberValidation.isComplete) {
    // Pending or incomplete = invalid for send mode
    Logger::errorln(F("Cannot send incomplete number: %s"), _state.currentDialingNumber);
    _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
    setState(AppState::InvalidNumber);
    _state.currentDialingNumber[0] = '\0';
    if (_integrationManager) {
      _integrationManager->updateDialingProgress("");
    }
    return IntegrationCallbackResult(false, "Incomplete number", "WEB_INCOMPLETE_NUMBER");
  }

  // Execute the action based on validation result
  switch (numberValidation.action) {
  case NumberAction::SystemAction:
    if (strEqual(_state.currentDialingNumber, kResetNumber)) {
      _modem.enqueueTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
      ESP.restart();
    } else if (strEqual(_state.currentDialingNumber, kFactoryResetNumber)) {
      _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kResetToneDuration);
      performFactoryReset();
    } else if (strEqual(_state.currentDialingNumber, kWifiWebPortalNumber)) {
      _state.isMaintenanceMode = !_state.isMaintenanceMode;
      onMaintenanceModeChanged(_state.isMaintenanceMode);
    }
    break;

  case NumberAction::QuickDial:
  case NumberAction::DirectDial:
    _modem.enqueueCall(numberValidation.targetNumber.c_str());
    break;

  case NumberAction::Invalid:
    _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
    setState(AppState::InvalidNumber);
    _state.currentDialingNumber[0] = '\0';
    if (_integrationManager) {
      _integrationManager->updateDialingProgress("");
    }
    return IntegrationCallbackResult(false, "Invalid number", "WEB_INVALID_NUMBER");

  default:
    break;
  }

  _state.currentDialingNumber[0] = '\0';
  if (_integrationManager) {
    _integrationManager->updateDialingProgress("");
  }

  return IntegrationCallbackResult(true);
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
      _state.newAppState == AppState::IncomingCallRing ||
      _state.newAppState == AppState::InvalidNumber) {
    _modem.hangUp();
    return IntegrationCallbackResult(true);
  }
  if (_state.currentDialingNumber[0] != '\0') {
    Logger::infoln(F("No active call - clearing dialing buffer"));
    _modem.stopTone();
    _state.currentDialingNumber[0] = '\0';
    if (_integrationManager) {
      _integrationManager->updateDialingProgress("");
    }
    return IntegrationCallbackResult(true);
  }
  String error = "No active call (state: " + String(appStateToString(_state.newAppState)) + ")";
  Logger::errorln(F("Integration hangup request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationRingRequest(const String &pattern,
                                                                   bool bypassDnd) {
  Logger::infoln(F("Integration ring request: %s (bypass DND: %s)"),
                 pattern.c_str(),
                 bypassDnd ? "yes" : "no");
  if (_state.newAppState == AppState::Idle) {
    if (pattern.isEmpty()) {
      String configPattern = _deviceConfig.getRingPattern();
      _ringer.startRinging(configPattern, bypassDnd);
    } else {
      _ringer.startRinging(pattern, bypassDnd);
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
  String error =
      "No active call with call waiting (state: " + String(appStateToString(_state.newAppState)) +
      ", has waiting: " + String(_state.callState.hasCallWaiting() ? "yes" : "no") + ")";
  Logger::errorln(F("Integration call waiting request: %s"), error.c_str());
  return IntegrationCallbackResult(false, error);
}

IntegrationCallbackResult TsuryPhone::handleIntegrationVolumeModeRequest(VolumeMode mode) {
  Logger::infoln(F("Integration volume mode request: %s"),
                 mode == VolumeMode::Speaker ? "speaker" : "earpiece");

  if (_state.newAppState != AppState::InCall) {
    String error = "Volume mode changes require an active call (current state: " +
                   String(appStateToString(_state.newAppState)) + ")";
    Logger::errorln(F("Integration volume mode request: %s"), error.c_str());
    return IntegrationCallbackResult(false, error, "PHONE_NOT_IN_CALL");
  }

  const VolumeMode currentMode = _modem.getCurrentVolumeMode();
  if (currentMode == mode) {
    Logger::infoln(F("Volume mode already %s"),
                   mode == VolumeMode::Speaker ? "speaker" : "earpiece");
    return IntegrationCallbackResult(true);
  }

  if (mode == VolumeMode::Speaker) {
    _modem.setSpeakerVolume();
  } else {
    _modem.setEarpieceVolume();
  }

  _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kVolumeToggleToneDurationMs);

  return IntegrationCallbackResult(true);
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
  case ConfigChangeEvent::DEFAULT_DIALING_CODE_CHANGED:
    Logger::infoln(F("Default dialing code updated to +%s"),
                   _deviceConfig.getDefaultDialingCode().c_str());
    break;
  default:
    break;
  }
}

#endif // HOME_ASSISTANT_INTEGRATION
