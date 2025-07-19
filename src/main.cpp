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
}

PhoneApp::PhoneApp()
    : _deviceConfig(),
      _deviceStats(),
      _state{},
      _modem(_deviceConfig),
      _ringer(),
      _hookSwitch(),
      _rotaryDial(),
      _wifi(_deviceConfig),
      _timeManager(_deviceConfig),
      _numberHandler(_deviceConfig),
      _integrationManager(_deviceConfig, _deviceStats, _state) {}

void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);

  Logger::infoln(F("TsuryPhone starting..."));

  _deviceConfig.init();
  _deviceStats.init();
  _wifi.init();
  _modem.init();
  _ringer.init();
  _rotaryDial.init();
  _hookSwitch.init();
  _timeManager.init();

  // Initialize integration manager (supports multiple integrations)
  if (_integrationManager.init()) {
    if (_integrationManager.hasEnabledIntegrations()) {
      Logger::infoln(F("Device integrations initialized"));
      _integrationManager.listIntegrations();
    }

    // Set up device operation callbacks for all integrations
    _integrationManager.setDialCallback(
        [this](const String &number) -> bool { return handleIntegrationDialRequest(number); });

    _integrationManager.setAnswerCallback(
        [this]() -> bool { return handleIntegrationAnswerRequest(); });

    _integrationManager.setHangupCallback(
        [this]() -> bool { return handleIntegrationHangupRequest(); });

    _integrationManager.setRingCallback(
        [this](const String &pattern) -> bool { return handleIntegrationRingRequest(pattern); });

    _integrationManager.setCallWaitingCallback(
        [this]() -> bool { return handleIntegrationCallWaitingRequest(); });

    _integrationManager.setCallBlockedCallback(
        [this](const String &number) { handleCallBlocked(number); });

    _integrationManager.setMaintenanceModeChangedCallback(
        [this](bool enabled) { onMaintenanceModeChanged(enabled); });
  } else {
    Logger::errorln(F("Failed to initialize Integration Manager"));
  }

  // Set up device config callback for organic/physical device events
  _deviceConfig.setConfigChangeCallback(
      [this](ConfigChangeType changeType) { handleConfigChange(changeType); });

  // Set up wifi portal timeout callback to exit maintenance mode
  _wifi.setPortalTimeoutCallback([this]() {
    if (_state.isMaintenanceMode) {
      _state.isMaintenanceMode = false;
      onMaintenanceModeChanged(_state.isMaintenanceMode);
    }
  });

  Logger::infoln(F("TsuryPhone started!"));

  setState(AppState::CheckHardware);
}

void PhoneApp::loop() {
#ifdef DEBUG
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    SerialAT.write(c);
  }
#endif

  // This is not the best way to do this, but I prefered it to having the state
  // change inside the ringer loop, and have a designated state (e.g. AfterFirstRing)
  // to handle this.
  // Basically I need to know as soon as the first ring is over, so I can play the
  // MP3 for the caller, to fit between the first and second ring.
  // The MP3 is not played immediately, to not surprise the user.
  const bool prevRangAtLeastOnce = _state.callState.rangAtLeastOnce;

  _modem.deriveStateFromMessage(_state);

  _modem.process(_state);
  _wifi.process();
  _hookSwitch.process();
  _rotaryDial.process(_state);
  _ringer.process(_state);
  _timeManager.process(_state);
  _integrationManager.process();

  const bool afterFirstRing = !prevRangAtLeastOnce && _state.callState.rangAtLeastOnce;

  if (afterFirstRing || _state.prevAppState != _state.newAppState) {
    onStateChanged();
  }

  processState();
}

void PhoneApp::setState(const AppState newState) {
  if (_state.newAppState == newState) {
    return;
  }

  _state.prevAppState = _state.newAppState;
  _state.newAppState = newState;

  onStateChanged();
}

void PhoneApp::onStateChanged() {
  if (_state.newAppState == _state.prevAppState) {
    Logger::infoln(F("Retrying state %s"), appStateToString(_state.newAppState));
  } else {
    Logger::infoln(F("Changing state from %s to %s"),
                   appStateToString(_state.prevAppState),
                   appStateToString(_state.newAppState));
  }

  // Note: State changes are now automatically detected by IntegrationManager

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

void PhoneApp::onStateCheckHardware() {
  _modem.sendCheckHardwareCommand();
}

void PhoneApp::onStateCheckLine() {
  _modem.sendCheckLineCommand();
}

void PhoneApp::onStateIdle() {
  stopEverything();
  _modem.setSpeakerVolume();

  if (_state.callState.otherPartyDropped) {
    _modem.enqueueTone(Tone::CallWaitingTone, kCallDroppedToneDuration);
  }

  if (!_firstTimeSystemReady) {
    _firstTimeSystemReady = true;
    _modem.enqueueMp3(state_ready);

    Logger::infoln(F("System ready!"));
  }
}

void PhoneApp::onStateIncomingCall() {
  CallState &callState = _state.callState;
  char *callNumber = callState.callNumber;

  // Record incoming call statistics when we first get the call number
  if (callNumber[0] != '\0' && !callState.introducedCaller) {
    // Note: Call blocking is now handled automatically by IntegrationManager
    // Note: Call start and call info are now handled automatically by IntegrationManager
  }

  if (callNumber[0] != '\0' && !callState.introducedCaller && callState.rangAtLeastOnce) {
    callState.introducedCaller = true;

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
    String ringPattern = _deviceConfig.getRingPattern();
    _ringer.startRinging(ringPattern);
  }
}

void PhoneApp::processStateInvalidNumber() {
  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
    setState(AppState::Idle);
  }
}

void PhoneApp::stopEverything() {
  Logger::infoln(F("Stopping everything..."));
  _modem.stopAllAudio();
  _ringer.stopRinging();
  // Reset current dialing number in state
  _state.currentDialingNumber[0] = '\0';
}

void PhoneApp::onStateInCall() {
  stopEverything();
  _modem.setEarpieceVolume();
  // Note: Call start recording is now handled automatically by StatsManager
}

void PhoneApp::processState() {
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

void PhoneApp::processStateCheckHardware() {
  if (millis() - _stateTime > kCheckHardwareTimeout) {
    onStateChanged();
  }
}

void PhoneApp::processStateCheckLine() {
  if (millis() - _stateTime > kCheckLineTimeout) {
    onStateChanged();
  }
}

void PhoneApp::processStateIdle() {
  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
  } else if (_hookSwitch.justChangedOffHook()) {
    _modem.enqueueTone(Tone::DialTone, kDialToneDuration);
  }

  if (_hookSwitch.isOffHook()) {
    int dialedDigit = _rotaryDial.getDialedDigit();
    char *dialedNumber = _state.currentDialingNumber;

    if (dialedDigit != kInvalidDialedDigit) {
      _modem.stopTone();
      Logger::infoln(F("Dialed digit: %d"), dialedDigit);
      Logger::infoln(F("Dialed number: %s"), dialedNumber);

      _modem.enqueueMp3(dialedDigitsToMp3s[dialedDigit]);

      // Note: Dialing progress is now handled automatically by IntegrationManager

      const NumberValidationResult numberValidation = _numberHandler.validateNumber(dialedNumber);

      if (numberValidation.isComplete) {
        switch (numberValidation.action) {
        case NumberAction::SystemAction:
          if (strEqual(dialedNumber, kResetNumber)) {
            _modem.enqueueTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
            ESP.restart();
          } else if (strEqual(dialedNumber, kWifiWebPortalNumber)) {
            // Toggle maintenance mode - update state directly
            _state.isMaintenanceMode = !_state.isMaintenanceMode;
            onMaintenanceModeChanged(_state.isMaintenanceMode);
          }
          break;

        case NumberAction::QuickDial:
        case NumberAction::DirectDial:
          _modem.enqueueCall(numberValidation.targetNumber.c_str());
          // Note: Call start and call info are now handled automatically by IntegrationManager
          break;
        case NumberAction::WebhookTrigger:
          // Trigger webhook HTTP call directly
          Logger::infoln(F("Webhook trigger: %s"), numberValidation.webhookId.c_str());
          _integrationManager.triggerWebhook(numberValidation.webhookId);
          break;

        case NumberAction::Invalid:
          _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
          setState(AppState::InvalidNumber);
          break;

        default:
          break;
        }

        // Reset current dialing number in state
        _state.currentDialingNumber[0] = '\0';
      } else if (numberValidation.action == NumberAction::Pending) {
        // Continue waiting for more digits
      }
    }
  }
}

void PhoneApp::processStateIncomingCall() {
  if (_hookSwitch.justChangedOffHook()) {
    _modem.answer();
  }
}

void PhoneApp::processStateDialing() {
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
    // Note: Call end is now handled automatically by IntegrationManager
  }
}

void PhoneApp::processStateInCall() {
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
    // Note: Call end is now handled automatically by IntegrationManager
  }

  const int dialedDigit = _rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kToggleVolumeToneDuration);
    _modem.toggleVolume();
  } else if (dialedDigit == 2 && _state.callState.hasCallWaiting()) {
    _modem.switchToCallWaiting();
  }

  if (_state.callState.hasCallWaiting() && !_state.callState.playedCallWaitingTone) {
    _modem.enqueueTone(Tone::IndianDialTone, kCallWaitingToneDuration);
    _state.callState.playedCallWaitingTone = true;
  }

  // Reset current dialing number in state
  _state.currentDialingNumber[0] = '\0';
}

// Integration Callback Methods
bool PhoneApp::handleIntegrationDialRequest(const String &number) {
  Logger::infoln(F("Integration dial request: %s"), number.c_str());

  // Only allow dialing when idle and off-hook
  if (_state.newAppState == AppState::Idle && _hookSwitch.isOffHook()) {
    // Use NumberHandler to validate the number
    NumberValidationResult validation = _numberHandler.validateNumber(number.c_str());

    if (validation.isComplete && (validation.action == NumberAction::QuickDial ||
                                  validation.action == NumberAction::DirectDial)) {
      _modem.enqueueCall(validation.targetNumber.c_str());
      return true;
    } else {
      Logger::errorln(F("Integration dial request: Invalid number %s"), number.c_str());
      return false;
    }
  } else {
    Logger::errorln(F("Integration dial request: Phone not ready (state: %s, hook: %s)"),
                    appStateToString(_state.newAppState),
                    _hookSwitch.isOffHook() ? "off" : "on");
    return false;
  }
}

bool PhoneApp::handleIntegrationAnswerRequest() {
  Logger::infoln(F("Integration answer request"));

  // Only allow answering during incoming call states
  if (_state.newAppState == AppState::IncomingCall ||
      _state.newAppState == AppState::IncomingCallRing) {
    _modem.answer();
    return true;
  } else {
    Logger::errorln(F("Integration answer request: No incoming call (state: %s)"),
                    appStateToString(_state.newAppState));
    return false;
  }
}

bool PhoneApp::handleIntegrationHangupRequest() {
  Logger::infoln(F("Integration hangup request"));

  // Allow hangup in any active call state
  if (_state.newAppState == AppState::InCall || _state.newAppState == AppState::Dialing ||
      _state.newAppState == AppState::IncomingCall ||
      _state.newAppState == AppState::IncomingCallRing) {

    _modem.hangUp();
    return true;
  } else {
    Logger::errorln(F("Integration hangup request: No active call (state: %s)"),
                    appStateToString(_state.newAppState));
    return false;
  }
}

bool PhoneApp::handleIntegrationRingRequest(const String &pattern) {
  Logger::infoln(F("Integration ring request: %s"), pattern.c_str());

  // Only allow ringing when idle
  if (_state.newAppState == AppState::Idle) {
    if (pattern.isEmpty()) {
      // Use default ring pattern from config
      String configPattern = _deviceConfig.getRingPattern();
      _ringer.startRinging(configPattern);
    } else {
      // Use specified pattern
      _ringer.startRinging(pattern);
    }
    return true;
  } else {
    Logger::errorln(F("Integration ring request: Phone not idle (state: %s)"),
                    appStateToString(_state.newAppState));
    return false;
  }
}

bool PhoneApp::handleIntegrationCallWaitingRequest() {
  Logger::infoln(F("Integration call waiting request"));

  // Only allow call waiting switch during an active call with call waiting available
  if (_state.newAppState == AppState::InCall && _state.callState.hasCallWaiting()) {
    _modem.switchToCallWaiting();
    return true;
  } else {
    Logger::errorln(F("Integration call waiting request: No active call with call waiting (state: "
                      "%s, has waiting: %s)"),
                    appStateToString(_state.newAppState),
                    _state.callState.hasCallWaiting() ? "yes" : "no");
    return false;
  }
}

// Call Blocking Callback
void PhoneApp::handleCallBlocked(const String &number) {
  Logger::infoln(F("Blocking call from: %s"), number.c_str());

  // Block the call by not answering and hanging up
  _modem.hangUp();
  setState(AppState::Idle);
}

void PhoneApp::handleConfigChange(ConfigChangeType changeType) {
  // Note: IntegrationManager now handles notifying integrations automatically

  // Handle specific config changes that affect the device directly
  switch (changeType) {
  case ConfigChangeType::Audio:
    onAudioConfigChanged();
    break;
  default:
    // Other config changes don't need specific handling
    break;
  }
}

void PhoneApp::onAudioConfigChanged() {
  Logger::infoln(F("Audio configuration changed, updating modem settings"));

  // Reapply the current volume mode with new settings from config
  if (_modem.getCurrentVolumeMode() == VolumeMode::Earpiece) {
    _modem.setEarpieceVolume();
  } else {
    _modem.setSpeakerVolume();
  }
}

void PhoneApp::onMaintenanceModeChanged(const bool enabled) {
  Logger::infoln(F("Maintenance mode changed to: %s"), enabled ? "enabled" : "disabled");

  // Notify integrations about the maintenance mode change
  // _integrationManager.onConfigurationChanged();

  _modem.enqueueTone(Tone::GeneralBeep, kWifiPortalToneDuration);

  if (enabled) {
    // Entering maintenance mode - open config portal
    Logger::infoln(F("Opening WiFi config portal"));
    _wifi.openConfigPortal();
  } else {
    // Exiting maintenance mode - close config portal
    Logger::infoln(F("Closing WiFi config portal"));
    _wifi.closeConfigPortal();
  }
}