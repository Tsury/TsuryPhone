#include "main.h"
#include "common/logger.h"
#include "common/phoneBook.h"
#include "common/string.h"
#include "generated/phoneBook.h"

namespace {
  const constexpr int kSerialBaudRate = kModemBaudRate;
  const constexpr int kCheckHardwareTimeout = 1500;
  const constexpr int kCheckLineTimeout = 1500;
  const constexpr int kCallDroppedToneDuration = 5000;
  const constexpr int kDialToneDuration = 100000;
  const constexpr int kResetToneDuration = 500;
  const constexpr int kWifiPortalToneDuration = 500;
  const constexpr int kToggleVolumeToneDuration = 75;
  const constexpr int kCallWaitingToneDuration = 500;
  const constexpr int kInvalidNumberMp3RepeatCount = 100;
}

PhoneApp::PhoneApp() : _modem(), _ringer(), _hookSwitch(), _rotaryDial(), _wifi() {}

void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);

  Logger::infoln(F("TsuryPhone starting..."));

  _wifi.init();
  _modem.init();
  _ringer.init();
  _rotaryDial.init();
  _hookSwitch.init();
  _timeManager.init();

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
  bool prevRangAtLeastOnce = _state.callState.rangAtLeastOnce;

  _modem.deriveStateFromMessage(_state);

  _modem.process(_state);
  _wifi.process();
  _hookSwitch.process();
  _rotaryDial.process();
  _ringer.process(_state);
  _timeManager.process(_state);

  bool afterFirstRing = !prevRangAtLeastOnce && _state.callState.rangAtLeastOnce;

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
    _modem.playTone(Tone::CallWaitingTone, kCallDroppedToneDuration);
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
    _ringer.startRinging();
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
  _modem.stopPlaying();
  _modem.stopTone();
  _ringer.stopRinging();
  _rotaryDial.resetCurrentNumber();
}

void PhoneApp::onStateInCall() {
  stopEverything();
  _modem.setEarpieceVolume();
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
    _modem.playTone(Tone::DialTone, kDialToneDuration);
  }

  if (_hookSwitch.isOffHook()) {
    DialedNumberResult dialedNumberResult = _rotaryDial.getCurrentNumber();
    char *dialedNumber = dialedNumberResult.callNumber;

    if (dialedNumberResult.dialedDigit != kInvalidDialedDigit) {
      _modem.stopTone();
      Logger::infoln(F("Dialed digit: %d"), dialedNumberResult.dialedDigit);
      Logger::infoln(F("Dialed number: %s"), dialedNumber);

      _modem.enqueueMp3(dialedDigitsToMp3s[dialedNumberResult.dialedDigit]);
    }

    DialedNumberValidationResult dialedNumberValidation = validateDialedNumber(dialedNumber);

    if (dialedNumberValidation == DialedNumberValidationResult::Valid) {
      if (strEqual(dialedNumber, kResetNumber)) {
        _modem.playTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
        ESP.restart();
      } else if (strEqual(dialedNumber, kWifiWebPortalNumber)) {
        _modem.playTone(Tone::GeneralBeep, kWifiPortalToneDuration);
        _wifi.openConfigPortal();
      } else {
        const char *numberToDial = isPhoneBookEntry(dialedNumber)
                                       ? getPhoneBookNumberForEntry(dialedNumber)
                                       : dialedNumber;
        _modem.enqueueCall(numberToDial);

        _rotaryDial.resetCurrentNumber();
      }
    } else if (dialedNumberValidation == DialedNumberValidationResult::Invalid) {
      _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
      setState(AppState::InvalidNumber);
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
  }
}

void PhoneApp::processStateInCall() {
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
  }

  int dialedDigit = _rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    _modem.playTone(Tone::PositiveAcknowledgeTone, kToggleVolumeToneDuration);
    _modem.toggleVolume();
  } else if (dialedDigit == 2 && _state.callState.hasCallWaiting()) {
    _modem.switchToCallWaiting();
  }

  if (_state.callState.hasCallWaiting() && !_state.callState.playedCallWaitingTone) {
    _modem.playTone(Tone::IndianDialTone, kCallWaitingToneDuration);
    _state.callState.playedCallWaitingTone = true;
  }

  _rotaryDial.resetCurrentNumber();
}