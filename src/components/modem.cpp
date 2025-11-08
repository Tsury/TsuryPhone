#include "modem.h"
#include "common/logger.h"
#include "common/stream.h"
#include "common/string.h"
#include "core/DeviceConfig.h"

namespace {
  constexpr std::array<const char *, 9> knownMessages = {"ATE0",
                                                         "+CCMXPLAY:",
                                                         "+CCMXSTOP:",
                                                         "+CSMS: 1,1,1",
                                                         "NO CARRIER",
                                                         "+CPIN: READY",
                                                         "SMS DONE",
                                                         "PB DONE",
                                                         "+CPIN: SIM REMOVED"};

  const constexpr uint16_t kModemResetDelay = 1500;

  const constexpr uint32_t kInitTimeoutMs = 5000UL;
  const constexpr uint32_t kKeepAliveIntervalMs = 30000UL;
  const constexpr uint32_t kKeepAliveTimeoutMs = 5000UL;
  const constexpr uint8_t kKeepAliveMaxRetries = 1;

  const constexpr uint16_t kModemHardResetRetries = 5;
  const constexpr uint32_t kModemHardResetRetryDelay = 500UL;
  const constexpr uint32_t kModemHardResetTimeoutMs = 12000UL;

  const constexpr uint16_t kResetSettleMs = 10;
  const constexpr uint16_t kResetPullMs = 1600;

  const constexpr uint16_t kPwrKeyLowMs = 70;
  const constexpr uint16_t kPwrKeyHighMs = 120;

  const constexpr uint16_t kProbeRespTimeoutMs = 200;
  const constexpr uint16_t kProbeRetryDelayMs = 150;

  // A safety margin between audio plays to prevent conflicts.
  const constexpr uint16_t kIntervalBetweenAudioPlaysMillis = 40;
}

bool Modem::probeOK(uint32_t timeoutMs) {
  uint32_t start = millis();

  unsigned long originalTimeout = SerialAT.getTimeout();
  SerialAT.setTimeout(kProbeRespTimeoutMs);

  bool found = false;

  while (millis() - start < timeoutMs) {
    SerialAT.print(F("AT\r"));

    if (SerialAT.find((char *)"OK")) {
      Logger::infoln(F("Modem OK! Took %lu ms"), millis() - start);
      found = true;
      break;
    }

    delay(kProbeRetryDelayMs);
  }

  SerialAT.setTimeout(originalTimeout);

  return found;
}

Modem::Modem(DeviceConfig &config)
    : _modemImpl(SerialAT), _waitingForKeepAlive(false), _lastKeepAliveSent(0UL), _config(config) {}

void Modem::init() {
  Logger::infoln(F("Initializing modem..."));

  SerialAT.begin(kModemBaudRate, SERIAL_8N1, kModemRxPin, kModemTxPin);

  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);

  initModem();

  Logger::infoln(F("Modem initialized!"));
}

void Modem::initModem() {
  bool modemUp = false;
  uint8_t tries = 0;

  while (!modemUp && tries < kModemHardResetRetries) {
    ++tries;
    Logger::infoln(F("Modem start attempt %u…\n"), tries);

    hardResetModem();

    modemUp = probeOK(kModemHardResetTimeoutMs);

    if (!modemUp) {
      Logger::warnln(F("No OK - retrying…"));
      delay(kModemHardResetRetryDelay);
    }
  }

  if (modemUp) {
    Logger::infoln(F("Modem ready!"));
  } else {
    Logger::errorln(F("Modem unreachable - rebooting MCU in 5 s"));
    ESP.restart();
  }

  disableUnneededFeatures();
  enableHangUp();
  stopAllAudio();
}

void Modem::hardResetModem() {
  pinMode(kModemResetPin, OUTPUT);
  digitalWrite(kModemResetPin, !kModemResetLevel);
  delay(kResetSettleMs);
  digitalWrite(kModemResetPin, kModemResetLevel);
  delay(kResetPullMs);
  digitalWrite(kModemResetPin, !kModemResetLevel);

  pinMode(kBoardPowerKeyPin, OUTPUT);
  digitalWrite(kBoardPowerKeyPin, LOW);
  delay(kPwrKeyLowMs);
  digitalWrite(kBoardPowerKeyPin, HIGH);
  delay(kPwrKeyHighMs);
  digitalWrite(kBoardPowerKeyPin, LOW);
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

  // Disables unsolicited PDP context event notifications.
  sendCommand(F("+CGEREP=0"));

  // Disables Extended Discontinuous Reception (eDRX) to ensure
  // immediate network responsiveness for voice calls.
  sendCommand(F("+CEDRXS=0"));

  // Disables unsolicited Sim Toolkit Proactive Commands (STK) notifications.
  sendCommand(F("+MSTK=0,0"));

  // Disables DTR.AT
  sendCommand(F("&D0"));

  // Disables UART sleep.
  sendCommand(F("+CSCLK=0"));
}

void Modem::disableUnneededFeaturesAfterInit() {
  // Disables connected line identification presentation notifications.
  sendCommand(F("+COLP=0"));
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
  const AudioConfig &audioConfig = _config.getAudioConfig();
  setVolume(audioConfig.earpieceVolume);
  setMicGain(audioConfig.earpieceGain);
}

void Modem::setSpeakerVolume() {
  _volumeMode = VolumeMode::Speaker;
  const AudioConfig &audioConfig = _config.getAudioConfig();
  setVolume(audioConfig.speakerVolume);
  setMicGain(audioConfig.speakerGain);
}

void Modem::toggleVolume() {
  if (_volumeMode == VolumeMode::Earpiece) {
    setSpeakerVolume();
  } else {
    setEarpieceVolume();
  }
}

void Modem::toggleMute() {
  if (_isMuted) {
    unmute();
  } else {
    mute();
  }
}

void Modem::mute() {
  Logger::infoln(F("Muting microphone"));
  sendCommand(F("+CMUT=1"));
  _isMuted = true;
}

void Modem::unmute() {
  Logger::infoln(F("Unmuting microphone"));
  sendCommand(F("+CMUT=0"));
  _isMuted = false;
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

void Modem::rejectCallWaiting(CallState &callState) {
  if (callState.waiting.number[0] != '\0') {
    Logger::warnln(F("Rejecting blocked waiting call %s"), callState.waiting.number);
  } else {
    Logger::warnln(F("Rejecting blocked waiting call (unknown number)"));
  }

  const int waitingCallId = callState.waiting.id;

  sendCommand(F("+CHLD=0"));
  verifyCallState();

  clearCallWaitingState(callState);

  if (waitingCallId != -1) {
    callState.waitingReleaseId = waitingCallId;
  }
}

bool Modem::sendDTMFTone(char digit) {
  Logger::infoln(F("Sending DTMF tone: %c"), digit);

  // Validate digit (should already be validated by caller, but double-check)
  if ((digit < '0' || digit > '9') && digit != '*' && digit != '#') {
    Logger::errorln(F("Invalid DTMF digit: %c"), digit);
    return false;
  }

  // AT+CLDTMF: Play local DTMF tone for user feedback
  // Format: AT+CLDTMF=<n>,<DTMF string>,<timeBase>,<path>
  // n=1: number of times to play (1-100)
  // DTMF string: "0-9,A-D,*,#" (quoted)
  // timeBase=100: duration in ms (50-500, default 100)
  // path=0: local output to earpiece/speaker
  char cldtmfCmd[32];
  snprintf(cldtmfCmd, sizeof(cldtmfCmd), "+CLDTMF=1,\"%c\",250,0", digit);
  sendCommand(cldtmfCmd);

  // Small delay to ensure local feedback starts before remote transmission
  delay(20);

  // AT+VTS: Send DTMF tone to remote party during call
  // Duration is fixed at ~150ms per A76XX spec
  // Special chars * and # need to be quoted
  char vtsCmd[16];
  if (digit == '*' || digit == '#') {
    snprintf(vtsCmd, sizeof(vtsCmd), "+VTS=\"%c\"", digit);
  } else {
    snprintf(vtsCmd, sizeof(vtsCmd), "+VTS=%c", digit);
  }
  sendCommand(vtsCmd);

  Logger::infoln(F("DTMF tone %c sent successfully"), digit);
  return true;
}

void Modem::clearCallWaitingState(CallState &callState) {
  callState.clearWaiting();
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
    if (_keepAliveRetryCount > 0) {
      Logger::infoln(F("Keep-alive received after %lu ms"), millis() - _lastKeepAliveSent);
    }

    _waitingForKeepAlive = false;
    _keepAliveRetryCount = 0;
  }

  if (Modem::isKnownMessage(msg) || strStartsWith(msg, "VOICE CALL:") ||
      strStartsWith(msg, "+CCWA")) {
    return;
  }

  snprintf(state.lastModemMessage, kBigBufferSize, "%s", msg);

  Logger::debugln(F("Modem: %s"), msg);

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

      // TODO: I didn't want to add side effects to this function, but this is kinda tame.
      // The "correct" way would be to do this from the main loop - onStateIdle should do this
      // if the previous state was AppState::CheckLine.
      disableUnneededFeaturesAfterInit();
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
    case 0: {
      // Active
      CallLeg *leg = nullptr;
      if (callState.active.id == callId) {
        leg = &callState.active;
      } else if (callState.waiting.id == callId) {
        callState.promoteWaitingToActive(millis());
        leg = &callState.active;
      } else {
        leg = &callState.active;
        leg->reset();
        leg->id = callId;
        leg->setNumber(callNumber);
      }
      leg->isOnHold = false;
      leg->shouldReject = false;
      leg->isIncoming = (callDirection == 1);
      if (leg->startedAtMs == 0UL) {
        leg->startedAtMs = millis();
      }
      state.newAppState = AppState::InCall;
      break;
    }
    case 1: {
      // Held
      CallLeg *leg = nullptr;
      if (callState.active.id == callId) {
        leg = &callState.active;
      } else {
        if (callState.waiting.id != callId) {
          callState.waiting.id = callId;
          callState.waiting.setNumber(callNumber);
        }
        leg = &callState.waiting;
      }
      leg->isOnHold = true;
      break;
    }
    case 2:
      // Dialing
      state.newAppState = AppState::Dialing;
      if (callState.active.id != callId) {
        callState.active.reset();
      }
      callState.active.id = callId;
      callState.active.setNumber(callNumber);
      callState.active.isPriority = false;
      callState.active.isBlocked = false;
      callState.active.isIncoming = false;
      callState.active.startedAtMs = 0UL;
      break;
    case 3:
      // Alerting (other party needs to pick up)
      break;
    case 4:
      // Incoming (doesn't include call waiting)
      state.newAppState = AppState::IncomingCall;
      if (callState.active.id != callId) {
        callState.active.reset();
      }
      callState.active.id = callId;
      callState.active.setNumber(callNumber);
      callState.active.isIncoming = true;
      callState.active.isBlocked = _config.isIncomingCallBlocked(callNumber);
      if (callState.active.isBlocked) {
        Logger::warnln(F("Incoming call %s is blocked"), callNumber);
        callState.active.isPriority = false;
      } else {
        callState.active.isPriority = _config.isPriorityCaller(callNumber);
      }
      callState.active.startedAtMs = 0UL;
      break;
    case 5:
      // Waiting
      if (callState.waiting.id != callId) {
        callState.waiting.reset();
      }
      callState.waiting.id = callId;
      callState.waiting.setNumber(callNumber);
      callState.waiting.isIncoming = true;
      callState.waiting.isBlocked = _config.isIncomingCallBlocked(callNumber);
      callState.waitingReleaseId = -1;

      if (callState.waiting.isBlocked) {
        Logger::warnln(F("Waiting call %s is blocked"), callNumber);
      }

      callState.waiting.isPriority =
          !callState.waiting.isBlocked && _config.isPriorityCaller(callNumber);
      callState.waiting.shouldReject = callState.waiting.isBlocked;
      callState.waiting.isOnHold = false;
      callState.waiting.startedAtMs = 0UL;
      break;
    case 6:
      // Since at least one party dropped, reset the call waiting tone state.
      callState.playedCallWaitingTone = false;

      // Disconnected (by the other party)
      if (callState.active.id == callId) {
        Logger::infoln(F("Current call %d was disconnected by the other party."), callId);

        if (callState.waiting.isOnHold) {
          Logger::infoln(F("Switching to call waiting %d..."), callState.waiting.id);

          callState.waiting.isOnHold = false;
          callState.promoteWaitingToActive(millis());
          callState.clearWaiting();
          switchToCallWaiting();
        } else {
          state.newAppState = AppState::Idle;
          state.callState = CallState{};
          state.callState.active.otherPartyDropped = prevAppState == AppState::InCall;
        }
      } else if (callState.waiting.id == callId) {
        Logger::infoln(F("Call waiting %d was disconnected by the other party."), callId);
        callState.clearWaiting();
      } else if (callState.waitingReleaseId == callId) {
        Logger::infoln(F("Blocked waiting call %d was rejected."), callId);
        callState.waitingReleaseId = -1;
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

void Modem::process(State &state) {
  state.volumeMode = _volumeMode;

  playNextAudioItem();
  callPending();

  // TODO: Maybe only call this when state is after AppState::CheckLine.
  keepAliveWatchdog();

  if (state.messageHandled || state.lastModemMessage[0] == '\0') {
    return;
  }

  if (strStartsWith(state.lastModemMessage, "+AUDIOSTATE: ")) {
    if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play stop")) {
      Logger::debugln(F("Audio stopped."));
      _isPlayingAudio = false;
      _lastAudioStopMillis = millis();
    } else if (strEqual(state.lastModemMessage, "+AUDIOSTATE: audio play")) {
      Logger::debugln(F("Audio playing..."));
      _isPlayingAudio = true;
    }
  } else if (strEqual(state.lastModemMessage, "+STTONE: 0")) {
    Logger::debugln(F("Tone stopped."));
    _isPlayingAudio = false;
    _lastAudioStopMillis = millis();
  } else {
    if (!strEqual(state.lastModemMessage, "OK")) {
      Logger::infoln(F("Unknown message: %s"), state.lastModemMessage);
    }
  }
}

void Modem::keepAliveWatchdog() {
  const uint32_t now = millis();
  const uint32_t timeSinceLastKeepAlive = now - _lastKeepAliveSent;

  if (_waitingForKeepAlive) {
    if (timeSinceLastKeepAlive > kKeepAliveTimeoutMs) {
      _waitingForKeepAlive = false;
      ++_keepAliveRetryCount;

      if (_keepAliveRetryCount > kKeepAliveMaxRetries) {
        Logger::warnln(F("Keep-alive timed out %u times -> hard reset"), _keepAliveRetryCount);
        reset();
        _keepAliveRetryCount = 0;
      } else {
        Logger::warnln(F("Keep-alive timeout (%u/%u). Retrying soon..."),
                       _keepAliveRetryCount,
                       kKeepAliveMaxRetries + 1);
      }
    }
    return;
  }

  if (timeSinceLastKeepAlive >= kKeepAliveIntervalMs) {
    sendKeepAlive();
    _lastKeepAliveSent = now;
    _waitingForKeepAlive = true;
  }
}

void Modem::sendKeepAlive() {
  if (_keepAliveRetryCount > 0) {
    Logger::infoln(F("Sending keep-alive (resets=%lu retries=%u)..."),
                   _watchdogResetCounter,
                   _keepAliveRetryCount);
  }
  _modemImpl.sendAT("");
}

void Modem::reset() {
  Logger::warnln(F("No keep-alive - resetting modem (%lu)..."), ++_watchdogResetCounter);

  initModem();
  _waitingForKeepAlive = false;
  _lastKeepAliveSent = millis();

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
