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
      _modem(_deviceConfig),
      _ringer(),
      _hookSwitch(),
      _rotaryDial(),
      _wifi(),
      _timeManager(_deviceConfig),
      _numberHandler(_deviceConfig),
      _integrationManager(_deviceConfig, _deviceStats) {}

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

    _integrationManager.setWebhookCallback([this](const String &webhookId) -> bool {
      return handleIntegrationWebhookTrigger(webhookId);
    });

    _integrationManager.setCallWaitingCallback(
        [this]() -> bool { return handleIntegrationCallWaitingRequest(); });

    // Set up config change callback to notify integrations and handle specific changes
    _deviceConfig.setConfigChangeCallback([this](ConfigChangeType changeType) {
      _integrationManager.onConfigurationChanged();

      // Handle specific config changes
      switch (changeType) {
      case ConfigChangeType::Audio:
        onAudioConfigChanged();
        break;
      case ConfigChangeType::MaintenanceMode:
        onMaintenanceModeChanged();
        break;
      default:
        // Other config changes don't need specific handling
        break;
      }
    });
  } else {
    Logger::errorln(F("Failed to initialize Integration Manager"));
  }

  // Set up wifi config portal timeout callback
  _wifi.setConfigPortalTimeoutCallback([this]() {
    // When portal times out, exit maintenance mode
    if (_deviceConfig.isMaintenanceMode()) {
      _deviceConfig.setMaintenanceMode(false);
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
  _rotaryDial.process();
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

  // Report state change to HA integration
  _integrationManager.updatePhoneState(_state.newAppState, _state.prevAppState);

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
    // Check if the incoming call is blocked
    if (_deviceConfig.isIncomingCallBlocked(String(callNumber))) {
      _deviceStats.recordBlockedCall(String(callNumber));
      _integrationManager.reportBlockedCall(String(callNumber));
      // Block the call by not answering and hanging up
      _modem.hangUp();
      setState(AppState::Idle);
      return;
    }

    _deviceStats.recordIncomingCall(String(callNumber));
    _integrationManager.reportCallStart(String(callNumber), true);
    _integrationManager.updateCallInfo(String(callNumber), true);
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
    _integrationManager.updateRingState(true);
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
  _integrationManager.updateRingState(false);
  _rotaryDial.resetCurrentNumber();
}

void PhoneApp::onStateInCall() {
  stopEverything();
  _modem.setEarpieceVolume();
  _deviceStats.recordCallStart();
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
    DialedNumberResult dialedNumberResult = _rotaryDial.getCurrentNumber();
    char *dialedNumber = dialedNumberResult.callNumber;

    if (dialedNumberResult.dialedDigit != kInvalidDialedDigit) {
      _modem.stopTone();
      Logger::infoln(F("Dialed digit: %d"), dialedNumberResult.dialedDigit);
      Logger::infoln(F("Dialed number: %s"), dialedNumber);

      _modem.enqueueMp3(dialedDigitsToMp3s[dialedNumberResult.dialedDigit]);

      // Report dialing progress to HA
      _integrationManager.updateDialingProgress(String(dialedNumber));
    }

    const NumberValidationResult numberValidation = _numberHandler.validateNumber(dialedNumber);

    if (numberValidation.isComplete) {
      switch (numberValidation.action) {
      case NumberAction::SystemAction:
        if (strEqual(dialedNumber, kResetNumber)) {
          _modem.enqueueTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
          ESP.restart();
        } else if (strEqual(dialedNumber, kWifiWebPortalNumber)) {
          // Toggle maintenance mode - the callback will handle portal control
          bool newMaintenanceMode = !_deviceConfig.isMaintenanceMode();
          _deviceConfig.setMaintenanceMode(newMaintenanceMode);
        }
        break;

      case NumberAction::QuickDial:
      case NumberAction::DirectDial:
        _deviceStats.recordOutgoingCall(numberValidation.targetNumber);
        _integrationManager.reportCallStart(numberValidation.targetNumber, false);
        _integrationManager.updateCallInfo(numberValidation.targetNumber, false);
        _modem.enqueueCall(numberValidation.targetNumber.c_str());
        _rotaryDial.resetCurrentNumber();
        break;
      case NumberAction::WebhookTrigger:
        // Trigger webhook in HA integration for automation processing
        Logger::infoln(F("Webhook trigger: %s"), numberValidation.webhookId.c_str());
        _integrationManager.reportWebhookTrigger(numberValidation.webhookId);
        _rotaryDial.resetCurrentNumber();
        break;

      case NumberAction::Invalid:
        _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
        setState(AppState::InvalidNumber);
        break;

      default:
        break;
      }
    } else if (numberValidation.action == NumberAction::Pending) {
      // Continue waiting for more digits
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
    unsigned long callDuration = _deviceStats.isCallActive() ? (millis() - _stateTime) / 1000 : 0;
    _deviceStats.recordCallEnd();
    _integrationManager.reportCallEnd(callDuration);
    _modem.hangUp();
  }
}

void PhoneApp::processStateInCall() {
  if (_hookSwitch.justChangedOnHook()) {
    unsigned long callDuration = _deviceStats.isCallActive() ? (millis() - _stateTime) / 1000 : 0;
    _deviceStats.recordCallEnd();
    _integrationManager.reportCallEnd(callDuration);
    _modem.hangUp();
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

  _rotaryDial.resetCurrentNumber();
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
      _deviceStats.recordOutgoingCall(validation.targetNumber);
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

    if (_deviceStats.isCallActive()) {
      _deviceStats.recordCallEnd();
    }
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

bool PhoneApp::handleIntegrationWebhookTrigger(const String &webhookId) {
  Logger::infoln(F("Integration webhook trigger: %s"), webhookId.c_str());

  // Report webhook trigger to HA integration for automation processing
  _integrationManager.reportWebhookTrigger(webhookId);
  return true;
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

void PhoneApp::onAudioConfigChanged() {
  Logger::infoln(F("Audio configuration changed, updating modem settings"));

  // Reapply the current volume mode with new settings from config
  if (_modem.getCurrentVolumeMode() == VolumeMode::Earpiece) {
    _modem.setEarpieceVolume();
  } else {
    _modem.setSpeakerVolume();
  }
}

void PhoneApp::onMaintenanceModeChanged() {
  bool maintenanceMode = _deviceConfig.isMaintenanceMode();
  Logger::infoln(F("Maintenance mode changed to: %s"), maintenanceMode ? "enabled" : "disabled");

  if (maintenanceMode) {
    // Entering maintenance mode - open config portal
    Logger::infoln(F("Opening WiFi config portal"));
    _modem.enqueueTone(Tone::GeneralBeep, kWifiPortalToneDuration);
    _wifi.openConfigPortal();
  } else {
    // Exiting maintenance mode - close config portal
    Logger::infoln(F("Closing WiFi config portal"));
    _modem.enqueueTone(Tone::GeneralBeep, kWifiPortalToneDuration);
    _wifi.closeConfigPortal();
  }
}