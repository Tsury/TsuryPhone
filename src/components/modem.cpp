
#include "modem.h"
#include "common/logger.h"
#include "common/stream.h"
#include "common/string.h"

namespace {
  constexpr std::array<const char *, 5> knownMessages = {
      "ATE0", "+CCMXPLAY:", "+CCMXSTOP:", "+CSMS: 1,1,1", "NO CARRIER"};

// Currently when developing and restarting a lot, I'm not interested
// in restarting the modem every time. This low delay prevents it from
// restarting when the modem is already on.
#ifdef DEBUG
  const constexpr int kModemResetDelay = 50;
  const constexpr int kDebugModemInitializationDelay = 1750;
#else
  const constexpr int kModemResetDelay = 1500;
#endif

  const constexpr int kGenericDelay = 50;
}

Modem::Modem() : _modemImpl(SerialAT) {}

void Modem::init() {
  Logger::infoln(F("Initializing modem..."));

  startModem();
  disableUnneededFeatures();
  enableHangUp();
  stopTone();

  Logger::infoln(F("Modem initialized!"));
}

void Modem::startModem() {
  SerialAT.begin(kModemBaudRate, SERIAL_8N1, kModemRxPin, kModemTxPin);

#ifdef BOARD_POWERON_PIN
  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

  pinMode(kModemResetPin, OUTPUT);
  digitalWrite(kModemResetPin, kModemResetLevel);
  delay(kModemResetDelay);
  digitalWrite(kModemResetPin, !kModemResetLevel);

  pinMode(kBoardPowerKeyPin, OUTPUT);
  digitalWrite(kBoardPowerKeyPin, HIGH);
  delay(kGenericDelay);
  digitalWrite(kBoardPowerKeyPin, LOW);

  while (!_modemImpl.testAT(kGenericDelay)) {
    delay(kGenericDelay);
  }

// Seems like this delay is only required when debugging - it's related to the
// serial monitor. When not using this delay, a modem error occurs on startup.
#ifdef DEBUG
  delay(kDebugModemInitializationDelay);
#endif
}

void Modem::disableUnneededFeatures() {
  // Disables command echo.
  sendCommand(F("E0"));

  // Disables enhanced SMS messaging services and advanced features,
  // reverting the modem to basic SMS mode.
  sendCommand(F("+CSMS=0"));

  // Disables unsolicited new message indications.
  sendCommand(F("+CNMI=0,0,0,0,0"));

  // Disables network registration unsolicited result codes (GSM).
  sendCommand(F("+CREG=0"));

  // Disables GPRS registration unsolicited result codes.
  sendCommand(F("+CGREG=0"));

  // Disables EPS (E-UTRAN) network registration unsolicited result codes.
  sendCommand(F("+CEREG=0"));

  // Disables connected line identification presentation notifications.
  sendCommand(F("+COLP=0"));

  // Disables unsolicited PDP context event notifications.
  sendCommand(F("+CGEREP=0"));
  ;
}

void Modem::enableHangUp() {
  sendCommand(F("+CVHU=0"));
}

void Modem::setVolume(const int volume) {
  sendCommand(F("+COUTGAIN=%d"), volume);
}

void Modem::setMicGain(const int gain) {
  sendCommand(F("+CMICGAIN=%d"), gain);
}

void Modem::setEarpieceVolume() {
  _volumeMode = VolumeMode::Earpiece;
  setVolume(kEarpieceVolume);
  setMicGain(kEarpieceMicGain);
}

void Modem::setSpeakerVolume() {
  _volumeMode = VolumeMode::Speaker;
  setVolume(kSpeakerVolume);
  setMicGain(kSpeakerMicGain);
}

void Modem::toggleVolume() {
  if (_volumeMode == VolumeMode::Earpiece) {
    setSpeakerVolume();
  } else {
    setEarpieceVolume();
  }
}

void Modem::enqueueCall(const char *number) {
  if (_enqueuedCall[0] != '\0') {
    Logger::warnln(F("Call already enqueued!"));
    return;
  }

  snprintf(_enqueuedCall, kSmallBufferSize, "%s", number);
}

void Modem::call(const char *number) {
  Logger::infoln(F("Dialing number: %s"), number);

  _modemImpl.callNumber(number);
  verifyCallState();
}

void Modem::hangUp() {
  Logger::infoln(F("Hanging up..."));

  _modemImpl.callHangup();
  verifyCallState();
}

void Modem::answer() {
  Logger::infoln(F("Answering call..."));

  _modemImpl.callAnswer();
  verifyCallState();
}

void Modem::switchToCallWaiting() {
  Logger::infoln(F("Switching to call waiting..."));

  sendCommand(F("+CHLD=2"));
  verifyCallState();
}

void Modem::verifyCallState() {
  sendCommand(F("+CPAS"));
}

bool Modem::messageAvailable() const {
  return SerialAT.available() > 0;
}

bool Modem::isKnownMessage(const char *msg) const {
  return std::any_of(knownMessages.begin(), knownMessages.end(), [msg](const char *known) {
    return strEqual(msg, known);
  });
}

void Modem::deriveStateFromMessage(State &state) {
  state.prevAppState = state.newAppState;
  state.lastModemMessage[0] = '\0';
  state.messageHandled = true;

  if (!messageAvailable()) {
    return;
  }

  char msg[kBigBufferSize];
  readLineFromStream(SerialAT, msg, kBigBufferSize);
  strTrim(msg);

  if (msg[0] == '\0') {
    return;
  }

  if (Modem::isKnownMessage(msg) || strStartsWith(msg, "VOICE CALL:") ||
      strStartsWith(msg, "+CCWA")) {
    return;
  }

  snprintf(state.lastModemMessage, kBigBufferSize, "%s", msg);

  Logger::infoln(F("Received from modem: %s"), msg);

  AppState prevAppState = state.prevAppState;
  CallState &callState = state.callState;

  if (strEqual(msg, "OK") && prevAppState == AppState::CheckHardware) {
    state.newAppState = AppState::CheckLine;
  } else if (strStartsWith(msg, "+CGREG") && prevAppState == AppState::CheckLine) {
    int status = -1;
    int n = -1;

    sscanf(msg, "+CGREG: %d,%d", &status, &n);

    // 0,1 means registered, home network
    if (status == 0 && n == 1) {
      state.newAppState = AppState::Idle;
    }
  } else if (strStartsWith(msg, "+CLCC") &&
             (prevAppState == AppState::Idle || prevAppState == AppState::IncomingCall ||
              prevAppState == AppState::IncomingCallRing || prevAppState == AppState::InCall ||
              prevAppState == AppState::Dialing)) {
    int callId = -1;
    int callDirection = -1;
    int callStatus = -1;
    int callMode = -1;
    int callMpty = -1;
    char callNumber[kSmallBufferSize] = {0};

    sscanf(msg,
           "+CLCC: %d,%d,%d,%d,%d,\"%31[^\"]\"",
           &callId,
           &callDirection,
           &callStatus,
           &callMode,
           &callMpty,
           callNumber);

    char buffer[kBigBufferSize];

    snprintf(buffer,
             sizeof(buffer),
             "Call ID: %d, Call Direction: %d, Call Status: %d, Call Mode: %d, "
             "Call Mpty: %d, Call Number: %s",
             callId,
             callDirection,
             callStatus,
             callMode,
             callMpty,
             callNumber);

    Logger::infoln(buffer);

    switch (callStatus) {
    case 0:
      // Active
      state.newAppState = AppState::InCall;
      callState.setcallNumber(callNumber);
      callState.callId = callId;
      break;
    case 1:
      // Held
      callState.isCallWaitingOnHold = true;
      callState.callWaitingId = callId;
      break;
    case 2:
      // Dialing
      state.newAppState = AppState::Dialing;
      break;
    case 3:
      // Alerting (other party needs to pick up)
      break;
    case 4:
      // Incoming (doesn't include call waiting)
      state.newAppState = AppState::IncomingCall;
      callState.setcallNumber(callNumber);
      callState.callId = callId;
      break;
    case 5:
      // Waiting
      callState.callWaitingId = callId;
      break;
    case 6:
      // Disconnected (by the other party)
      // TODO: I have chosen not to handle the very rare case of having the other party disconnect
      // the incoming call, all the while there's a call waiting (not on hold).
      if (callState.callId == callId) {
        Logger::infoln(F("Current call %d was disconnected by the other party."), callId);

        if (callState.isCallWaitingOnHold) {
          Logger::infoln(F("Switching to call waiting %d..."), callState.callWaitingId);

          callState.isCallWaitingOnHold = false;
          callState.callId = callState.callWaitingId;
          callState.callWaitingId = -1;
          switchToCallWaiting();
        } else {
          state.newAppState = AppState::Idle;
          state.callState = CallState{};
          state.callState.otherPartyDropped = true;
        }
      } else if (callState.callWaitingId == callId) {
        Logger::infoln(F("Call waiting %d was disconnected by the other party."), callId);
        callState.callWaitingId = -1;
        callState.isCallWaitingOnHold = false;
      } else {
        state.newAppState = AppState::Idle;
        state.callState = CallState{};
        Logger::warnln(F("Unknown call %d was disconnected by the other party."), callId);
      }
      break;
    default:
      Logger::warnln(F("Unknown call status: %d"), callStatus);
      break;
    }
  } else if (strStartsWith(msg, "RING")) {
    if ((prevAppState == AppState::IncomingCall || prevAppState == AppState::Idle)) {
      state.newAppState = AppState::IncomingCallRing;
    } else if (prevAppState == AppState::IncomingCallRing) {
      state.newAppState = AppState::IncomingCall;
    }
  } else if (strStartsWith(msg, "+CPAS") &&
             (prevAppState == AppState::IncomingCallRing ||
              prevAppState == AppState::IncomingCall || prevAppState == AppState::InCall ||
              prevAppState == AppState::Dialing)) {
    int callStatus = -1;

    sscanf(msg, "+CPAS: %d", &callStatus);

    Logger::infoln(F("Call Status: %d"), callStatus);

    switch (callStatus) {
    case 0:
      // Ready
      state.newAppState = AppState::Idle;
      state.callState = CallState{};
      break;
    case 3:
      // Ringing
      break;
    case 4:
      // Call in progress
      state.newAppState = AppState::InCall;
      break;
    }
  } else {
    state.messageHandled = false;
  }
}

void Modem::process(const State &state) {
  if (!_audioPlaying) {
    playPendingMp3();

    if (!_audioPlaying) {
      callPending();
    }
  }

  if (state.messageHandled || state.lastModemMessage[0] == '\0') {
    return;
  }

  if (strStartsWith(state.lastModemMessage, "+AUDIOSTATE: ")) {
    if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play stop")) {
      Logger::infoln(F("Audio stopped."));
      _audioPlaying = false;
    } else if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play")) {
      Logger::infoln(F("Audio playing..."));
      _audioPlaying = true;
    }
  } else if (strStartsWith(state.lastModemMessage, "AT+STTONE=1")) {
    int toneId = -1;
    int duration = -1;

    sscanf(state.lastModemMessage, "AT+STTONE=1,%d,%d", &toneId, &duration);
    _tonePlaying = true;

    Logger::infoln(F("Playing tone %d for %dms."), toneId, duration);
  } else if (strEqual(state.lastModemMessage, "+STTONE: 0")) {
    Logger::infoln(F("Tone stopped."));
    _tonePlaying = false;
  } else {
    if (!strEqual(state.lastModemMessage, "OK")) {
      Logger::infoln(F("Unknown message: %s"), state.lastModemMessage);
    }
  }
}

template <typename... Args>
inline void Modem::sendCommand(const __FlashStringHelper *command, Args... args) {
  char buffer[kMediumBufferSize];
  snprintf_P(buffer, sizeof(buffer), reinterpret_cast<PGM_P>(command), args...);
  sendCommand(buffer);
}

void Modem::sendCommand(const StringSumHelper &command) {
  Logger::infoln(F("Sending command: AT%s"), command.c_str());
  _modemImpl.sendAT(command);
}

void Modem::sendCommand(const __FlashStringHelper *command) {
  Logger::infoln(F("Sending command: AT%s"), command);
  _modemImpl.sendAT(command);
}

void Modem::sendCommand(const char *command) {
  Logger::infoln(F("Sending command: AT%s"), command);
  _modemImpl.sendAT(command);
}

void Modem::sendCheckHardwareCommand() {
  sendCommand(F(""));
}

void Modem::sendCheckLineCommand() {
  sendCommand(F("+CGREG?"));
}

void Modem::playTone(const Tone toneId, const int duration) {
  sendCommand(F("+STTONE=1,%d,%d"), toneId, duration);
}

void Modem::stopTone() {
  sendCommand(F("+STTONE=0"));
}

void Modem::playMp3(const char *fileName, const int repeat) {
  // Setting true and not waiting for the AUDIOSTATE message to make sure nothing
  // gets in the way of playing the MP3 (call/other MP3).
  _audioPlaying = true;

  Logger::infoln(F("Playing MP3: %s"), fileName);

  if (fileName != nullptr) {
    char playCmd[kBigBufferSize];
    snprintf(playCmd, sizeof(playCmd), "+CCMXPLAY=\"%s/%s\",0,%d", kMp3Dir, fileName, repeat);

    sendCommand(playCmd);

    Logger::infoln(F("Playing MP3: %s success!"), fileName);
  } else {
    Logger::errorln(F("Error: MP3File has a null fileName!"));
    _audioPlaying = false;
  }
}

void Modem::enqueueMp3(const char *file, const int repeat) {
  PendingMp3 pending = {file, repeat};
  _pendingMp3Queue.push(pending);
}

void Modem::playPendingMp3() {
  if (_pendingMp3Queue.empty()) {
    return;
  }

  Logger::infoln(F("Playing pending MP3..."));

  PendingMp3 pending = _pendingMp3Queue.front();
  _pendingMp3Queue.pop();
  playMp3(pending.filename, pending.repeat);
}

void Modem::callPending() {
  if (_enqueuedCall[0] == '\0') {
    return;
  }

  call(_enqueuedCall);
  _enqueuedCall[0] = '\0';
}

void Modem::stopPlaying() {
  if (!_audioPlaying) {
    return;
  }

  sendCommand(F("+CCMXSTOP"));
}