#include "main.h"
#include "common/common.h"
#include "common/helpers.h"
#include "common/logger.h"

namespace {
  constexpr int kSerialBaudRate = kModemBaudRate;
  constexpr int kCheckHardwareTimeout = 1500;
  constexpr int kCheckLineTimeout = 1500;
}

PhoneApp::PhoneApp() : modem(), ringer(), hookSwitch(), rotaryDial(), wifi() {}

void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);
  Logger::infoln(F("TsuryPhone starting..."));

  modem.init();
  ringer.init();
  rotaryDial.init();
  hookSwitch.init();
  wifi.init();

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

  ringer.process();
  wifi.process();
  hookSwitch.process();
  rotaryDial.process();

  StateResult stateResult = modem.deriveNewStateFromMessage(state);
  modem.process(stateResult);

  if (strlen(stateResult.callerNumber) > 0) {
    Logger::infoln(F("We have caller number for state: %s"),
                   appStateToString(stateResult.newState));
    Logger::infoln(F("Caller number: %s"), stateResult.callerNumber);
  }

  if (stateResult.newState != state) {
    setState(stateResult.newState, stateResult);
  }
}

void PhoneApp::setState(const AppState newState, const StateResult &result) {
#if DEBUG
  if (state == newState) {
    Logger::infoln(F("Retrying state %s"), appStateToString(state));
  } else {
    Logger::infoln(
        F("Changing state from %s to %s"), appStateToString(state), appStateToString(newState));
  }
#endif

  state = newState;
  stateTime = millis();

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
  modem.sendCheckHardwareCommand();
}

void PhoneApp::onSetStateCheckLineState() {
  modem.sendCheckLineCommand();
}

void PhoneApp::onSetStateIdleState() {
  ringer.stopRinging();

  if (firstTimeSystemReady) {
    return;
  }

  firstTimeSystemReady = true;
  modem.enqueueMp3(state_ready);
  // modem.playTone(1, 10000);
  Logger::infoln(F("System ready!"));
}

void PhoneApp::onSetStateIncomingCallState(const StateResult &result) {
  // TODO: BIG TODO!!
  // Need to only play the caller mp3 after the first ring!
  if (result.callerNumber[0] != '\0') {
    if (hasMp3ForCaller(result.callerNumber)) {
      Logger::infoln(F("Playing MP3 for caller: %s"), result.callerNumber);
      modem.setSpeakerVolume();
      const char *mp3Ptr = getMp3ForCaller(result.callerNumber);

      if (mp3Ptr != nullptr) {
        modem.enqueueMp3(mp3Ptr);
      } else {
        Logger::errorln(F("No MP3 for caller: %s"), result.callerNumber);
      }

      modem.setEarpieceVolume();
    } else {
      Logger::infoln(F("No MP3 for caller: %s"), result.callerNumber);
      // TODO: TTS?
    }
  } else {
    // We ring on both incoming call and incoming call ring states.
    ringer.startRinging();
  }
}

void PhoneApp::onSetStateIncomingCallRingState() {
  Logger::infoln(F("Ringing..."));
  ringer.startRinging();
}

void PhoneApp::onSetStateInCallState() {
  ringer.stopRinging();
  modem.setEarpieceVolume();
}

void PhoneApp::processState() {
  switch (state) {
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
  if (millis() - stateTime > kCheckHardwareTimeout) {
    setState(AppState::CheckHardware);
  }
}

void PhoneApp::processStateCheckLine() {
  if (millis() - stateTime > kCheckLineTimeout) {
    setState(AppState::CheckLine);
  }
}

void PhoneApp::processStateIdle() {
  if (hookSwitch.justChangedOnHook()) {
    rotaryDial.resetCurrentNumber();
    modem.stopPlaying();
  }

  if (hookSwitch.isOffHook()) {
    DialedNumberResult dialedNumberResult = rotaryDial.getCurrentNumber();
    char *dialedNumber = dialedNumberResult.callerNumber;

    if (dialedNumberResult.dialedDigit != 99) {
      Logger::infoln(F("Dialed digit: %d"), dialedNumberResult.dialedDigit);
      modem.enqueueMp3(dialedDigitsToMp3s[dialedNumberResult.dialedDigit]);

      Logger::infoln(F("Dialed number: %s"), dialedNumber);
    }

    DialedNumberValidationResult dialedNumberValidation = validateDialedNumber(dialedNumber);

    if (dialedNumberValidation != DialedNumberValidationResult::Pending) {
      if (dialedNumberValidation == DialedNumberValidationResult::Valid) {
        modem.enqueueCall(dialedNumber);
        rotaryDial.resetCurrentNumber();
      } else {
        modem.enqueueMp3(dial_error, 10);
        rotaryDial.resetCurrentNumber();
      }
    }
  }
}

void PhoneApp::processStateIncomingCall() {
  if (hookSwitch.justChangedOffHook()) {
    modem.answer();
  }
}

void PhoneApp::processStateDialing() {
  if (hookSwitch.justChangedOnHook()) {
    modem.hangUp();
  }
}

void PhoneApp::processStateInCall() {
  // TODO: This is copy-pasted from processStateDialing. Refactor!
  if (hookSwitch.justChangedOnHook()) {
    modem.hangUp();
  }

  int dialedDigit = rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    modem.toggleVolume();
  } else if (dialedDigit == 2) {
    modem.switchToCallWaiting();
  }

  rotaryDial.resetCurrentNumber();
}