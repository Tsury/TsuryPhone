#include "main.h"
#include "common/common.h"
#include "common/helpers.h"
#include "common/logger.h"

namespace {
  constexpr int kSerialBaudRate = kModemBaudRate;
  constexpr int kCheckHardwareTimeout = 1500;
  constexpr int kCheckLineTimeout = 1500;
}

PhoneApp::PhoneApp() : _modem(), _ringer(), _hookSwitch(), _rotaryDial(), _wifi() {}

void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);
  Logger::infoln(F("TsuryPhone starting..."));

  _modem.init();
  _ringer.init();
  _rotaryDial.init();
  _hookSwitch.init();
  _wifi.init();

  setState(AppState::CheckHardware);
}

void PhoneApp::loop() {
#if DEBUG
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    SerialAT.write(c);

    if (c == '\r') {
      Serial.write('\n');
      SerialAT.write('\n');
    }
  }
#endif

  processState();

  _ringer.process();
  _wifi.process();
  _hookSwitch.process();
  _rotaryDial.process();

  StateResult stateResult = _modem.deriveNewStateFromMessage(_state);
  _modem.process(stateResult);

  if (strlen(stateResult.callerNumber) > 0) {
    Logger::infoln(F("We have caller number for state: %s"),
                   appStateToString(stateResult.newState));
    Logger::infoln(F("Caller number: %s"), stateResult.callerNumber);
  }

  if (stateResult.newState != _state) {
    setState(stateResult.newState, stateResult);
  }
}

void PhoneApp::setState(const AppState newState, const StateResult &result) {
#if DEBUG
  if (_state == newState) {
    Logger::infoln(F("Retrying state %s"), appStateToString(_state));
  } else {
    Logger::infoln(
        F("Changing state from %s to %s"), appStateToString(_state), appStateToString(newState));
  }
#endif

  _state = newState;
  _stateTime = millis();

  switch (newState) {
  case AppState::CheckHardware:
    onSetStateCheckHardwareState();
    break;
  case AppState::CheckLine:
    onSetStateCheckLineState();
    break;
  case AppState::Idle:
    onSetStateIdleState();
    break;
  case AppState::IncomingCall:
    onSetStateIncomingCallState(result);
    break;
  case AppState::IncomingCallRing:
    onSetStateIncomingCallRingState();
    break;
  case AppState::InCall:
    onSetStateInCallState();
    break;
  case AppState::Dialing:
    break;
  case AppState::WifiTool:
    break;
  default:
    break;
  }
}

void PhoneApp::onSetStateCheckHardwareState() {
  _modem.sendCheckHardwareCommand();
}

void PhoneApp::onSetStateCheckLineState() {
  _modem.sendCheckLineCommand();
}

void PhoneApp::onSetStateIdleState() {
  _ringer.stopRinging();

  if (_firstTimeSystemReady) {
    return;
  }

  _firstTimeSystemReady = true;
  _modem.enqueueMp3(state_ready);
  // modem.playTone(1, 10000);
  Logger::infoln(F("System ready!"));
}

void PhoneApp::onSetStateIncomingCallState(const StateResult &result) {
  // TODO: BIG TODO!!
  // Need to only play the caller mp3 after the first ring!
  if (result.callerNumber[0] != '\0') {
    if (hasMp3ForCaller(result.callerNumber)) {
      Logger::infoln(F("Playing MP3 for caller: %s"), result.callerNumber);
      _modem.setSpeakerVolume();
      const char *mp3Ptr = getMp3ForCaller(result.callerNumber);

      if (mp3Ptr != nullptr) {
        _modem.enqueueMp3(mp3Ptr);
      } else {
        Logger::errorln(F("No MP3 for caller: %s"), result.callerNumber);
      }

      _modem.setEarpieceVolume();
    } else {
      Logger::infoln(F("No MP3 for caller: %s"), result.callerNumber);
      // TODO: TTS?
    }
  } else {
    // We ring on both incoming call and incoming call ring states.
    _ringer.startRinging();
  }
}

void PhoneApp::onSetStateIncomingCallRingState() {
  Logger::infoln(F("Ringing..."));
  _ringer.startRinging();
}

void PhoneApp::onSetStateInCallState() {
  _ringer.stopRinging();
  _modem.setEarpieceVolume();
}

void PhoneApp::processState() {
  switch (_state) {
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
  case AppState::WifiTool:
    break;
  default:
    break;
  }
}

void PhoneApp::processStateCheckHardware() {
  if (millis() - _stateTime > kCheckHardwareTimeout) {
    setState(AppState::CheckHardware);
  }
}

void PhoneApp::processStateCheckLine() {
  if (millis() - _stateTime > kCheckLineTimeout) {
    setState(AppState::CheckLine);
  }
}

void PhoneApp::processStateIdle() {
  if (_hookSwitch.justChangedOnHook()) {
    _rotaryDial.resetCurrentNumber();
    _modem.stopPlaying();
  }

  if (_hookSwitch.isOffHook()) {
    DialedNumberResult dialedNumberResult = _rotaryDial.getCurrentNumber();
    char *dialedNumber = dialedNumberResult.callerNumber;

    if (dialedNumberResult.dialedDigit != 99) {
      Logger::infoln(F("Dialed digit: %d"), dialedNumberResult.dialedDigit);
      _modem.enqueueMp3(dialedDigitsToMp3s[dialedNumberResult.dialedDigit]);

      Logger::infoln(F("Dialed number: %s"), dialedNumber);
    }

    DialedNumberValidationResult dialedNumberValidation = validateDialedNumber(dialedNumber);

    if (dialedNumberValidation != DialedNumberValidationResult::Pending) {
      if (dialedNumberValidation == DialedNumberValidationResult::Valid) {
        _modem.enqueueCall(dialedNumber);
        _rotaryDial.resetCurrentNumber();
      } else {
        _modem.enqueueMp3(dial_error, 10);
        _rotaryDial.resetCurrentNumber();
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
  }
}

void PhoneApp::processStateInCall() {
  // TODO: This is copy-pasted from processStateDialing. Refactor!
  if (_hookSwitch.justChangedOnHook()) {
    _modem.hangUp();
  }

  int dialedDigit = _rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    _modem.toggleVolume();
  } else if (dialedDigit == 2) {
    _modem.switchToCallWaiting();
  }

  _rotaryDial.resetCurrentNumber();
}