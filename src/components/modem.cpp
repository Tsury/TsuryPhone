#include "modem.h"
#include "common/logger.h"
#include "common/stream.h"
#include "common/string.h"

namespace {
  constexpr std::array<const char *, 5> knownMessages = {
      "ATE0", "+CCMXPLAY:", "+CCMXSTOP:", "+CSMS: 1,1,1", "NO CARRIER"};

  const constexpr int kModemResetDelay = 1500;
  const constexpr int kGenericDelay = 50;

  const constexpr uint32_t kKeepAliveIntervalMs = 30000UL;
  const constexpr uint32_t kKeepAliveTimeoutMs = 5000UL;

  // A safety margin between audio plays to prevent conflicts.
  static constexpr int kIntervalBetweenAudioPlaysMillis = 40;
}

Modem::Modem() : _modemImpl(SerialAT), _waitingForKeepAlive(false), _lastKeepAliveSent(0UL) {}

void Modem::init() {
  Logger::infoln(F("Initializing modem..."));

  startModem();

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

  disableUnneededFeatures();
  enableHangUp();
  stopAllAudio();
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

  // Disables Extended Discontinuous Reception (eDRX) to ensure
  // immediate network responsiveness for voice calls.
  sendCommand(F("+CEDRXS=0"));

  // Disables unsolicited Sim Toolkit Proactive Commands (STK) notifications.
  sendCommand(F("+MSTK=0,0"));
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

  if (strEqual(msg, "OK") && _waitingForKeepAlive) {
    _waitingForKeepAlive = false;
    Logger::infoln(F("Keep-alive received after %lu ms"), millis() - _lastKeepAliveSent);
  }

  if (Modem::isKnownMessage(msg) || strStartsWith(msg, "VOICE CALL:") ||
      strStartsWith(msg, "+CCWA")) {
    return;
  }

  snprintf(state.lastModemMessage, kBigBufferSize, "%s", msg);

  Logger::infoln(F("Received from modem: %s"), msg);

  const AppState prevAppState = state.prevAppState;
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
      // Since at least one party dropped, reset the call waiting tone state.
      callState.playedCallWaitingTone = false;

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
          state.callState.otherPartyDropped = prevAppState == AppState::InCall;
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
    default:
      Logger::warnln(F("Unknown call status: %d"), callStatus);
      break;
    }
  } else {
    state.messageHandled = false;
  }
}

void Modem::process(const State &state) {
  playNextAudioItem();
  callPending();

  // TODO: Maybe only call this when state is after AppState::CheckLine.
  keepAliveWatchdog();

  if (state.messageHandled || state.lastModemMessage[0] == '\0') {
    return;
  }

  if (strStartsWith(state.lastModemMessage, "+AUDIOSTATE: ")) {
    if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play stop")) {
      Logger::infoln(F("Audio stopped."));
      _isPlayingAudio = false;
      _lastAudioStopMillis = millis();
    } else if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play")) {
      Logger::infoln(F("Audio playing..."));
      _isPlayingAudio = true;
    }
  } else if (strEqual(state.lastModemMessage, "+STTONE: 0")) {
    Logger::infoln(F("Tone stopped."));
    _isPlayingAudio = false;
    _lastAudioStopMillis = millis();
  } else {
    if (!strEqual(state.lastModemMessage, "OK")) {
      Logger::infoln(F("Unknown message: %s"), state.lastModemMessage);
    }
  }
}

void Modem::keepAliveWatchdog() {
  uint32_t now = millis();
  uint32_t timeSinceLastKeepAlive = now - _lastKeepAliveSent;

  if (_waitingForKeepAlive) {
    if (timeSinceLastKeepAlive > kKeepAliveTimeoutMs) {
      Logger::warnln(F("No keep-alive response, resetting modem..."));
      resetModem();
    }
  } else {
    if (timeSinceLastKeepAlive >= kKeepAliveIntervalMs) {
      sendKeepAlive();
      _lastKeepAliveSent = now;
      _waitingForKeepAlive = true;
    }
  }
}

void Modem::sendKeepAlive() {
  Logger::infoln(F("Sending keep-alive. (Watchdog resets so far: %lu)"), _watchdogResetCounter);
  _modemImpl.sendAT("");
}

void Modem::resetModem() {
  startModem();
  _waitingForKeepAlive = false;
  _lastKeepAliveSent = millis();
  _watchdogResetCounter++;
  Logger::warnln(F("Modem has been reset via keep-alive watchdog."));
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

void Modem::enqueueTone(const Tone toneId, const int duration) {
  AudioItem item{AudioType::Tone, toneId, duration, nullptr, 0};
  _audioQueue.push(item);
}

void Modem::stopTone() {
  sendCommand(F("+STTONE=0"));
}

void Modem::enqueueMp3(const char *file, const int repeat) {
  AudioItem item{AudioType::Mp3, Tone::DialTone, 0, file, repeat};
  _audioQueue.push(item);
}

void Modem::stopMp3() {
  sendCommand(F("+CCMXSTOP"));
}

void Modem::stopAllAudio() {
  _audioQueue.clear();
  stopTone();
  stopMp3();
  _isPlayingAudio = false;
  _lastAudioStopMillis = 0UL;
}

void Modem::playTone(const Tone toneId, const int duration) {
  // We set this right away so the next item won't start
  _isPlayingAudio = true;
  sendCommand(F("+STTONE=1,%d,%d"), toneId, duration);
}

void Modem::playMp3(const char *fileName, const int repeat) {
  // We set this right away so the next item won't start
  _isPlayingAudio = true;

  Logger::infoln(F("Playing MP3: %s"), fileName);

  if (fileName != nullptr) {
    char playCmd[kBigBufferSize];
    snprintf(playCmd, sizeof(playCmd), "+CCMXPLAY=\"%s/%s\",0,%d", kMp3Dir, fileName, repeat);
    sendCommand(playCmd);

    Logger::infoln(F("Playing MP3: %s success!"), fileName);
  } else {
    Logger::errorln(F("Error: MP3File has a null fileName!"));
    _isPlayingAudio = false;
  }
}

bool Modem::hasAudioToPlay() {
  return !_audioQueue.empty();
}

void Modem::playNextAudioItem() {
  if (_isPlayingAudio) {
    return;
  }

  if (!hasAudioToPlay()) {
    return;
  }

  // Wait a bit between audio plays to prevent conflicts
  if (millis() - _lastAudioStopMillis < kIntervalBetweenAudioPlaysMillis) {
    return;
  }

  const AudioItem &nextItem = _audioQueue.front();

  switch (nextItem.type) {
  case AudioType::Tone:
    Logger::infoln(F("Playing queued tone..."));
    playTone(nextItem.toneId, nextItem.toneDuration);
    break;
  case AudioType::Mp3:
    Logger::infoln(F("Playing queued MP3..."));
    playMp3(nextItem.filename, nextItem.repeat);
    break;
  }

  // Once we've initiated play, pop it from the queue
  _audioQueue.pop();
}

void Modem::callPending() {
  if (_isPlayingAudio) {
    return;
  }

  if (_enqueuedCall[0] == '\0') {
    return;
  }

  call(_enqueuedCall);
  _enqueuedCall[0] = '\0';
}
