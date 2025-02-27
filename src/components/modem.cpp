
#include "modem.h"
#include "Arduino.h"
#include "common/common.h"
#include "common/logger.h"

// Currently when developing and restarting a lot, I'm not interested
// in restarting the modem every time. This low delay prevents it from
// restarting when the modem is already on.
namespace {
#ifdef DEBUG
constexpr int kModemResetDelay = 50;
constexpr int kDebugModemInitializationDelay = 1750;
#else
constexpr int kModemResetDelay = 1500;
#endif

constexpr int kGenericDelay = 50;
constexpr int kSendCommandBufferSize = 64;
} // namespace

Modem::Modem() : _modemImpl(SerialAT) {}

void Modem::init() {
  Logger::infoln(F("Initializing modem..."));

  SerialAT.begin(kModemBaudRate, SERIAL_8N1, kModemRxPin, kModemTxPin);

#ifdef BOARD_POWERON_PIN
  pinMode(kBoardPowerOnPin, OUTPUT);
  digitalWrite(kBoardPowerOnPin, HIGH);
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
  // serial monitor.
#ifdef DEBUG
  delay(kDebugModemInitializationDelay);
#endif

  disableUnneededFeatures();
  enableHangUp();

  Logger::infoln(F("Modem initialized!"));
}

void Modem::disableUnneededFeatures() {
  // Disable command echo.
  sendCommand(F("E0"));

  // Disable SMS service.
  sendCommand(F("+CSMS=0"));

  // Disable new SMS indications.
  sendCommand(F("+CNMI=0,0,0,0,0"));

  // Disable network registration unsolicited result code.
  sendCommand(F("+CREG=0"));

  // Disable GPRS unsolicited result code.
  sendCommand(F("+CGREG=0"));

  // Disable EPS network registration status unsolicited result code.
  sendCommand(F("+CEREG=0"));
}

void Modem::enableHangUp() {
  sendCommand(F("+CVHU=0"));
}

void Modem::setVolume(const int volume) {
  sendCommand(F("+COUTGAIN=%d"), volume);
}

void Modem::setEarpieceVolume() {
  _volumeMode = VolumeMode::Earpiece;
  setVolume(kEarpieceVolume);
}

void Modem::setSpeakerVolume() {
  _volumeMode = VolumeMode::Speaker;
  setVolume(kSpeakerVolume);
}

void Modem::toggleVolume() {
  if (_volumeMode == VolumeMode::Earpiece) {
    setSpeakerVolume();
  } else {
    setEarpieceVolume();
  }
}

void Modem::enqueueCall(const char *number) {
  if (strlen(_enqueuedCall) > 0) {
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
  if (_callState.callWaitingId == -1) {
    Logger::warnln(F("No call waiting to switch to!"));
    return;
  }

  Logger::infoln(F("Switching to call waiting %d..."), _callState.callWaitingId);
  sendCommand(F("+CHLD=2"));
  verifyCallState();
}

void Modem::verifyCallState() {
  sendCommand(F("+CPAS"));
}

bool Modem::messageAvailable() const {
  return SerialAT.available() > 0;
}

StateResult Modem::deriveNewStateFromMessage(const AppState &currState) {
  StateResult result = {currState, currState, "", "", true, false};

  if (!messageAvailable()) {
    return result;
  }

  char msg[kBigBufferSize];
  readLineFromStream(SerialAT, msg, kBigBufferSize);
  trimString(msg);

  if (strlen(msg) == 0) {
    return result;
  }

  snprintf(result.message, kBigBufferSize, "%s", msg);

  Logger::infoln(F("Received from modem: %s"), msg);

  if (strEqual(msg, "OK") && currState == AppState::CheckHardware) {
    result.newState = AppState::CheckLine;
  } else if (startsWith(msg, "+CGREG") && currState == AppState::CheckLine) {
    int status = -1;
    int n = -1;

    sscanf(msg, "+CGREG: %d,%d", &status, &n);

    if (status == 0 && n == 1) {
      result.newState = AppState::Idle;
    }
  } else if (startsWith(msg, "+CLCC") &&
             (currState == AppState::Idle || currState == AppState::IncomingCall ||
              currState == AppState::IncomingCallRing || currState == AppState::InCall ||
              currState == AppState::Dialing)) {
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
      // Call is active.
      result.newState = AppState::InCall;
      _callState =
          CallState{callNumber, callId, _callState.callWaitingId, _callState.isCallWaitingOnHold};
      break;
    case 1:
      Logger::infoln(F("Call %d is held."), callId);

      // Call is held.
      _callState.isCallWaitingOnHold = true;
      _callState.callWaitingId = callId;
      break;
    case 2:
      // Outgoing call initiated.
      result.newState = AppState::Dialing;
      break;
    case 3:
      // Outgoing call ringing.
      break;
    case 4:
      // Incoming call.
      // TODO: Does this trigger when receiving a call waiting?
      result.newState = AppState::IncomingCall;
      snprintf(result.callerNumber, kSmallBufferSize, "%s", callNumber);
      break;
    case 5:
      // Call waiting.
      _callState.callWaitingId = callId;
      result.callWaiting = true;
      break;
    case 6:
      // Call is disconnected.
      // TODO: Handle case when call is disconnected and we have a call waiting.
      if (_callState.callId == callId) {
        Logger::infoln(F("Current call %d is disconnected."), callId);

        if (_callState.isCallWaitingOnHold) {
          Logger::infoln(F("We have call waiting %d..."), _callState.callWaitingId);
          switchToCallWaiting();
          _callState.isCallWaitingOnHold = false;
          _callState.callId = _callState.callWaitingId;
          _callState.callWaitingId = -1;
          // TODO: Will CLCC take care of setting the new call number?
        } else {
          Logger::infoln(F("No call waiting."));
          result.newState = AppState::Idle;
          _callState = CallState{"", -1, -1, false};
        }
      } else if (_callState.callWaitingId == callId) {
        Logger::infoln(F("Call waiting %d is disconnected."), callId);
        _callState.callWaitingId = -1;
        _callState.isCallWaitingOnHold = false;
      } else {
        result.newState = AppState::Idle;

        // TODO: Probably only need to print here is if we were in an active
        // call, and not ringing/incoming call.
        Logger::warnln(F("Unknown call %d is disconnected."), callId);
      }
      break;
    default:
      Logger::warnln(F("Unknown call status: %d"), callStatus);
      break;
    }
  } else if (startsWith(msg, "RING")) {
    if ((currState == AppState::IncomingCall || currState == AppState::Idle)) {
      result.newState = AppState::IncomingCallRing;
    } else if (currState == AppState::IncomingCallRing) {
      result.newState = AppState::IncomingCall;
    }
  } else if (startsWith(msg, "+CPAS") &&
             (currState == AppState::IncomingCallRing || currState == AppState::IncomingCall ||
              currState == AppState::InCall || currState == AppState::Dialing)) {
    int callStatus = -1;

    sscanf(msg, "+CPAS: %d", &callStatus);

    // TODO: What does this show when we have a call waiting?
    Logger::infoln(F("Call Status: %d"), callStatus);

    switch (callStatus) {
    // CPAS values:
    // 0: Ready (not in call)
    // 3: Ringing
    // 4: Call in progress
    case 0:
      result.newState = AppState::Idle;
      break;
    case 4:
      result.newState = AppState::InCall;
      break;
    }
  } else {
    result.messageHandled = false;
  }

  return result;
}

void Modem::process(const StateResult &result) {
  if (!_audioPlaying) {
    playPendingMp3();

    if (!_audioPlaying) {
      callPending();
    }
  }

  if (!result.messageHandled && strlen(result.message) > 0) {
    if (startsWith(result.message, "+AUDIOSTATE: ")) {
      if (strEqual(result.message, "+AUDIOSTATE: audio play stop")) {
        Logger::infoln(F("Audio stopped."));
        _audioPlaying = false;
      } else if (strEqual(result.message, "+AUDIOSTATE: audio play")) {
        Logger::infoln(F("Audio playing..."));
        _audioPlaying = true;
      }
    } else if (startsWith(result.message, "AT+STTONE=1")) {
      int toneId = -1;
      int duration = -1;

      sscanf(result.message, "AT+STTONE=1,%d,%d", &toneId, &duration);
      _tonePlaying = true;

      Logger::infoln(F("Playing tone %d for %dms."), toneId, duration);
    } else if (strEqual(result.message, "+STTONE: 0")) {
      Logger::infoln(F("Tone stopped."));
      _tonePlaying = false;
    } else {
      // TODO: Maybe put these in some collection/function.
      if (!strEqual(result.message, "OK") && !strEqual(result.message, "ATE0") &&
          !strEqual(result.message, "+CCMXPLAY:") && !strEqual(result.message, "+CSMS: 1,1,1")) {
        Logger::infoln(F("Unknown message: %s"), result.message);
      }
    }
  }

  if (result.newState == result.prevState) {
    return;
  }

  if (result.newState == AppState::InCall) {
    setEarpieceVolume();
  }

  if (result.newState == AppState::Idle) {
    setSpeakerVolume();
  }
}

template <typename... Args>
inline void Modem::sendCommand(const __FlashStringHelper *command, Args... args) {
  char buffer[kSendCommandBufferSize];
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

void Modem::playTone(const int toneId, const int duration) {
  sendCommand(F("+STTONE=1,%d,%d"), toneId, duration);
}

void Modem::stopTone() {
  sendCommand(F("+STTONE=0"));
}

void Modem::playMp3(const char *fileName, const int repeat) {
  _audioPlaying = true;

  Logger::infoln(F("Playing MP3: %s"), fileName);

  if (fileName == nullptr) {
    _audioPlaying = false;
    Logger::errorln(F("Error: MP3File has a null fileName!"));
    return;
  }

  char playCmd[kBigBufferSize];
  snprintf(playCmd, sizeof(playCmd), "+CCMXPLAY=\"%s/%s\",0,%d", kMp3Dir, fileName, repeat);

  sendCommand(playCmd);

  Logger::infoln(F("Playing MP3: %s success!"), fileName);
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
  if (strlen(_enqueuedCall) == 0) {
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