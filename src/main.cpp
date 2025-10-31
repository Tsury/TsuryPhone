#include "main.h"
#include "common/logger.h"
#include "common/string.h"
#include "generated/mp3.h"

namespace {
  const constexpr int kSerialBaudRate = kModemBaudRate;
  const constexpr int kCheckHardwareTimeout = 1500;
  const constexpr int kCheckLineTimeout = 1500;
  const constexpr int kCallDroppedToneDuration = 3000;
  const constexpr int kDialToneDuration = 1000000;
  const constexpr int kResetToneDuration = 500;
  const constexpr int kWifiPortalToneDuration = 500;
  const constexpr int kToggleVolumeToneDuration = 75;
  const constexpr int kCallWaitingToneDuration = 500;
  const constexpr int kInvalidNumberMp3RepeatCount = 100;

  const constexpr uint32_t kRingerIntervalMs = 15;
  const constexpr uint32_t kTimeMgrIntervalMs = 50;
  const constexpr uint32_t kWifiIntervalMs = 120;
  const constexpr uint32_t kIntegrationIntervalMs = 40;

  inline bool due(uint32_t &last, uint32_t period, uint32_t now) {
    if (now - last >= period) {
      last = now;
      return true;
    }
    return false;
  }
} // namespace

TsuryPhone::TsuryPhone()
    : _deviceConfig(),
      _state{},
      _modem(_deviceConfig),
      _ringer(),
      _hookSwitch(),
      _rotaryDial(),
      _wifi(_deviceConfig),
      _timeManager(_deviceConfig),
      _numberHandler(_deviceConfig) {}

void TsuryPhone::setup() {
  Serial.begin(kSerialBaudRate);
  Logger::infoln(F("TsuryPhone starting..."));
  _deviceConfig.init();
  _wifi.init();
  _modem.init();
  _ringer.init();
  _rotaryDial.init();
  _hookSwitch.init();
  _state.isHookOff = _hookSwitch.isOffHook();
  _timeManager.init();
  _integrationManager.reset(new IntegrationManager(*this, _deviceConfig, _state));

  if (!_integrationManager->init()) {
    Logger::errorln(F("Failed to initialize Integration Manager"));
  }

#ifdef HOME_ASSISTANT_INTEGRATION
  _integrationManager->setupTsuryPhoneCallbacks(
      [this](const String &number) -> IntegrationCallbackResult {
        return handleIntegrationDialRequest(number);
      },
      [this](uint8_t digit, bool deferValidation) -> IntegrationCallbackResult {
        return handleIntegrationDialDigitRequest(digit, deferValidation);
      },
      [this]() -> IntegrationCallbackResult { 
        return handleIntegrationSendDialedNumberRequest(); 
      },
      [this]() -> IntegrationCallbackResult { return handleIntegrationAnswerRequest(); },
      [this]() -> IntegrationCallbackResult { return handleIntegrationHangupRequest(); },
      [this](const String &pattern, bool bypassDnd) -> IntegrationCallbackResult {
        return handleIntegrationRingRequest(pattern, bypassDnd);
      },
      [this]() -> IntegrationCallbackResult { return handleIntegrationCallWaitingRequest(); },
      [this](VolumeMode mode) -> IntegrationCallbackResult {
        return handleIntegrationVolumeModeRequest(mode);
      },
      [this](const String &number) { handleIntegrationCallBlocked(number); },
      [this](bool enabled) { handleIntegrationMaintenanceModeChanged(enabled); },
      [this]() { performFactoryReset(); },
      [this](ConfigChangeEvent event) { handleIntegrationConfigChanged(event); });
#endif
  _wifi.setPortalTimeoutCallback([this]() {
    if (_state.isMaintenanceMode) {
      _state.isMaintenanceMode = false;
      onMaintenanceModeChanged(_state.isMaintenanceMode);
    }
  });
  Logger::infoln(F("TsuryPhone started!"));
  setState(AppState::CheckHardware);
}

void TsuryPhone::loop() {
#ifdef DEBUG
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    SerialAT.write(c);
    if (_integrationManager) {
      _integrationManager->enqueueDebugChar(c);
    }
  }
#endif
  // NOTE (restored comment): This is not the best way to do this, but it was preferred over
  // introducing a separate transient state (e.g. AfterFirstRing) or adding state change logic
  // inside the ringer itself. We need to know as soon as the FIRST ring finishes so we can
  // schedule / enqueue the caller-specific MP3 to play cleanly between the first and second rings.
  // We capture the ring transition using a simple edge: store whether we had already rung at least
  // once before processing subsystems, then compare after ringer processing. The MP3 is NOT played
  // immediately at the edge to avoid surprising the user; instead the state handler uses the flag
  // to introduce the MP3 at a natural gap.
  const bool prevRangAtLeastOnce = _state.callState.active.rangAtLeastOnce;
  _modem.deriveStateFromMessage(_state);
  const uint32_t now = millis();
  _modem.process(_state);
  _hookSwitch.process(_state);
  _rotaryDial.process(_state);
  if (due(_lastRinger, kRingerIntervalMs, now)) {
    _ringer.process(_state);
  }
  if (due(_lastTimeMgr, kTimeMgrIntervalMs, now)) {
    _timeManager.process(_state);
  }
  if (due(_lastWifi, kWifiIntervalMs, now)) {
    _wifi.process(_state);
  }
  if (_integrationManager && due(_lastIntegration, kIntegrationIntervalMs, now)) {
    _integrationManager->process();
  }
  const bool afterFirstRing = !prevRangAtLeastOnce && _state.callState.active.rangAtLeastOnce;
  if (afterFirstRing || _state.prevAppState != _state.newAppState) {
    onStateChanged();
  }
  processState();
  vTaskDelay(1);
}

void TsuryPhone::setState(const AppState newState) {
  if (_state.newAppState == newState) {
    return;
  }
  _state.prevAppState = _state.newAppState;
  _state.newAppState = newState;
  onStateChanged();
}

void TsuryPhone::onStateChanged() {
  if (_state.newAppState == _state.prevAppState) {
    Logger::infoln(F("Retrying state %s"), appStateToString(_state.newAppState));
  } else {
    Logger::infoln(F("Changing state from %s to %s"),
                   appStateToString(_state.prevAppState),
                   appStateToString(_state.newAppState));
  }
  _stateTime = millis();
  switch (_state.newAppState) {
  case AppState::CheckHardware:
    onStateCheckHardware();
    break;
  case AppState::CheckLine:
    onStateCheckLine();
    break;
  case AppState::Idle:
    onStateIdle();
    break;
  case AppState::IncomingCall:
  case AppState::IncomingCallRing:
    onStateIncomingCall();
    break;
  case AppState::InCall:
    onStateInCall();
    break;
  case AppState::Dialing:
    break;
  default:
    break;
  }
}

void TsuryPhone::onStateCheckHardware() {
  _modem.sendCheckHardwareCommand();
}

void TsuryPhone::onStateCheckLine() {
  _modem.sendCheckLineCommand();
}

void TsuryPhone::onStateIdle() {
  stopEverything();
  _modem.setSpeakerVolume();

  if (_state.callState.active.otherPartyDropped) {
    _modem.enqueueTone(Tone::CallWaitingTone, kCallDroppedToneDuration);
  }

  if (!_firstTimeSystemReady) {
    _firstTimeSystemReady = true;
    _modem.enqueueMp3(state_ready);

    Logger::infoln(F("System ready!"));
  }
}

void TsuryPhone::onStateIncomingCall() {
  CallState &callState = _state.callState;
  char *callNumber = callState.active.number;

  // TODO: BUG - When a blocked number is dialing, the phone might ring for a split second.
  if (callState.active.isBlocked) {
    Logger::warnln(F("Dropping blocked incoming call %s"), callNumber);
    _ringer.stopRinging();
    _modem.hangUp();
    return;
  }

  if (callNumber[0] != '\0' && !callState.active.introducedCaller &&
      callState.active.rangAtLeastOnce) {
    callState.active.introducedCaller = true;

    if (hasMp3ForCall(callNumber)) {
      Logger::infoln(F("Playing MP3 for caller: %s"), callNumber);
      const char *mp3Ptr = getMp3ForCall(callNumber);

      if (mp3Ptr != nullptr) {
        _modem.enqueueMp3(mp3Ptr);
      } else {
        Logger::errorln(F("No MP3 for caller: %s"), callNumber);
      }
    } else {
      Logger::infoln(F("No MP3 for caller: %s"), callNumber);
      // TODO: TTS?
    }
  } else {
    // We ring on both incoming call and incoming call ring states.
    Logger::infoln(F("Ringing..."));

    if (_state.isDnd) {
      if (callState.active.isPriority) {
        Logger::infoln(F("Bypassing DND for priority caller %s"), callNumber);
      } else {
        Logger::infoln(F("Suppressing ring due to DND (caller %s not priority)"), callNumber);
        return;
      }
    }
    String ringPattern = _deviceConfig.getRingPattern();
    _ringer.startRinging(ringPattern, callState.active.isPriority);
  }
}

void TsuryPhone::processStateInvalidNumber() {
  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
    setState(AppState::Idle);
  }
}

void TsuryPhone::stopEverything() {
  Logger::infoln(F("Stopping everything..."));
  _modem.stopAllAudio();
  _ringer.stopRinging();
  // Reset current dialing number in state
  _state.currentDialingNumber[0] = '\0';
}

void TsuryPhone::performFactoryReset() {
  Logger::warnln(F("Factory reset initiated via system number"));

  stopEverything();

  if (_integrationManager) {
    _integrationManager->onFactoryResetInitiated();
  }

  _wifi.resetCredentials();

  if (!_deviceConfig.resetToFactoryDefaults()) {
    Logger::errorln(F("Factory reset may be incomplete: configuration file removal failed"));
  }

  if (_integrationManager) {
    _integrationManager->onFactoryResetBeforeRestart();
  }

  delay(kResetToneDuration + 500);
  ESP.restart();
}

void TsuryPhone::onStateInCall() {
  stopEverything();
  _modem.setEarpieceVolume();
}

void TsuryPhone::processState() {
  switch (_state.newAppState) {
  case AppState::CheckHardware:
    processStateCheckHardware();
    break;
  case AppState::CheckLine:
    processStateCheckLine();
    break;
  case AppState::Idle:
    processStateIdle();
    break;
  case AppState::InCall:
    processStateInCall();
    break;
  case AppState::Dialing:
    processStateDialing();
    break;
  case AppState::IncomingCall:
  case AppState::IncomingCallRing:
    processStateIncomingCall();
    break;
  case AppState::InvalidNumber:
    processStateInvalidNumber();
    break;
  default:
    break;
  }
}

void TsuryPhone::processStateCheckHardware() {
  if (millis() - _stateTime > kCheckHardwareTimeout) {
    onStateChanged();
  }
}

void TsuryPhone::processStateCheckLine() {
  if (millis() - _stateTime > kCheckLineTimeout) {
    onStateChanged();
  }
}

void TsuryPhone::processStateIdle() {
  if (_hookSwitch.justChangedOffHook()) {
    Logger::infoln(F("Handset lifted - starting dial tone"));
    stopEverything();
    _modem.enqueueTone(Tone::DialTone, kDialToneDuration);
    _state.currentDialingNumber[0] = '\0';
    return;
  }

  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
    return;
  }

  if (!_hookSwitch.isOffHook()) {
    return;
  }

  const int dialedDigit = _rotaryDial.getDialedDigit();
  if (dialedDigit >= 0) {
    // RotaryDial already appended the digit to the state buffer; avoid double-appending here.
    handleDialedDigitInput(static_cast<uint8_t>(dialedDigit), false, false);
  }
}

bool TsuryPhone::handleDialedDigitInput(uint8_t digit, bool appendToState, bool fromIntegration, bool skipValidation) {
  if (digit > 9) {
    return false;
  }

  if (appendToState) {
    size_t len = strlen(_state.currentDialingNumber);
    if (len >= sizeof(_state.currentDialingNumber) - 1) {
      Logger::warnln(F("Dial buffer full, cannot append digit %u"), static_cast<unsigned>(digit));
      return false;
    }
    _state.currentDialingNumber[len] = static_cast<char>('0' + digit);
    _state.currentDialingNumber[len + 1] = '\0';
  }

  _modem.stopTone();

  Logger::infoln(F("%s dialed digit: %u"),
                 fromIntegration ? "Integration" : "Rotary",
                 static_cast<unsigned>(digit));
  Logger::infoln(F("Dialed number: %s"), _state.currentDialingNumber);

  _modem.enqueueMp3(dialedDigitsToMp3s[digit]);

  // If skipValidation is true, just update state and return (for send mode)
  if (skipValidation) {
    if (_integrationManager) {
      _integrationManager->updateDialingProgress(_state.currentDialingNumber);
    }
    return true;
  }

  const String dialedString(_state.currentDialingNumber);
  bool handledAsAction = false;
  bool integrationPartialMatch = false;
  if (_integrationManager) {
    if (_integrationManager->isActionCode(dialedString)) {
      const String actionId = _integrationManager->resolveActionId(dialedString);
      if (!actionId.isEmpty()) {
        Logger::infoln(F("Action trigger %s"), actionId.c_str());
        _integrationManager->triggerAction(actionId);
        handledAsAction = true;
        _state.currentDialingNumber[0] = '\0';
      }
    } else if (_integrationManager->hasPartialActionMatch(dialedString)) {
      integrationPartialMatch = true;
    }
  }

  if (handledAsAction || integrationPartialMatch) {
    return true;
  }

  const NumberValidationResult numberValidation =
      _numberHandler.validateNumber(_state.currentDialingNumber);

  if (numberValidation.isComplete) {
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
      break;

    default:
      break;
    }

    _state.currentDialingNumber[0] = '\0';
  } else if (numberValidation.action == NumberAction::Pending) {
    // Waiting for more digits; keep current buffer.
  }

  return true;
}

void TsuryPhone::processStateIncomingCall() {
  if (_hookSwitch.justChangedOffHook()) {
    _modem.answer();
  }
}

void TsuryPhone::processStateDialing() {
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
  }
}

void TsuryPhone::processStateInCall() {
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
  }

  if (_state.callState.waiting.shouldReject && _state.callState.waiting.id != -1) {
    String waitingNumber = String(_state.callState.waiting.number);

    _state.callState.waiting.shouldReject = false;
    _modem.rejectCallWaiting(_state.callState);

    if (_integrationManager) {
      _integrationManager->handleCallBlocked(waitingNumber);
    }
  }

  const int dialedDigit = _rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kToggleVolumeToneDuration);
    _modem.toggleVolume();
  } else if (dialedDigit == 2 && _state.callState.hasCallWaiting()) {
    _state.callState.promoteWaitingToActive(millis());
    _modem.switchToCallWaiting();
  }

  if (_state.callState.hasCallWaiting() && !_state.callState.playedCallWaitingTone) {
    _modem.enqueueTone(Tone::IndianDialTone, kCallWaitingToneDuration);
    _state.callState.playedCallWaitingTone = true;
  }

  _state.currentDialingNumber[0] = '\0';
}

void TsuryPhone::onMaintenanceModeChanged(const bool enabled) {
  Logger::infoln(F("Maintenance mode changed to: %s"), enabled ? "enabled" : "disabled");
  // NOTE: We do NOT call onConfigurationChanged() here. Reasons:
  //   - Maintenance mode is a transient runtime flag (opens/closes WiFi config portal), not a
  //     persisted configuration field.
  //   - Forcing a full config diff broadcast would be semantically wrong and adds noise.
  // CURRENT BEHAVIOR (still integration-visible): Even without an explicit call here, the
  // IntegrationManager change detector notices _state.isMaintenanceMode changed and emits a
  // generic "state" event that includes isMaintenanceMode. So HA clients WILL still see the
  // toggle reflected in the regular phone state payload.
  // MISSING PARITY: When HA itself toggles maintenance via /api/config/maintenance we ALSO emit
  // a focused key change (broadcastStateChange("maintenance.enabled", ...)). Dial-originated
  // toggles do NOT currently emit that specific delta event; only the generic state snapshot.
  // FUTURE IMPROVEMENT: Add a dedicated lightweight helper, e.g.
  //     _integrationManager->notifyMaintenanceModeChanged(enabled);
  // that allows HAIntegration to mirror the focused key change event without a full config push.
  // Historical (disabled) broad notification retained for reference:
  // _integrationManager->onConfigurationChanged(); // intentionally disabled

  _modem.enqueueTone(Tone::GeneralBeep, kWifiPortalToneDuration);

  if (enabled) {
    Logger::infoln(F("Opening WiFi config portal"));
    _wifi.openConfigPortal();
  } else {
    Logger::infoln(F("Closing WiFi config portal"));
    _wifi.closeConfigPortal();
  }
}