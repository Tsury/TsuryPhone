
#include "modem.h"
#include "Arduino.h"
#include "common/common.h"
#include "common/logger.h"

#define GENERIC_DELAY 50
#define BAUD_RATE MODEM_BAUDRATE
#define CHECK_LINE_INTERVAL_MINUTES 60

#ifdef DEBUG
#define DEBUG_MODEM_INITIALIZATION_TIME 1750
#endif

Modem::Modem() : _modemImpl(SerialAT)
{
}

void Modem::init()
{
    Logger::info("Initializing modem...");

    SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(GENERIC_DELAY);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    delay(GENERIC_DELAY);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(GENERIC_DELAY);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(GENERIC_DELAY);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    while (!_modemImpl.testAT(GENERIC_DELAY))
    {
        digitalWrite(BOARD_PWRKEY_PIN, LOW);
        delay(GENERIC_DELAY);
        digitalWrite(BOARD_PWRKEY_PIN, HIGH);
        delay(GENERIC_DELAY);
        digitalWrite(BOARD_PWRKEY_PIN, LOW);
    }

#ifdef DEBUG
    delay(DEBUG_MODEM_INITIALIZATION_TIME);
#endif

    disableEcho();
    enableHangUp();

    Logger::info("Modem initialized!");

#if WRITE_MP3S
    writeMp3s();
#endif
}

void Modem::disableEcho()
{
    sendCommand("E0");
}

void Modem::enableHangUp()
{
    sendCommand("+CVHU=0");
}

void Modem::setVolume(const int volume)
{
    sendCommand(String("+COUTGAIN=") + volume);
}

void Modem::setEarpieceVolume()
{
    _volumeMode = VolumeMode::Earpiece;
    setVolume(EARPIECE_VOLUME);
}

void Modem::setSpeakerVolume()
{
    _volumeMode = VolumeMode::Speaker;
    setVolume(SPEAKER_VOLUME);
}

void Modem::toggleVolume()
{
    if (_volumeMode == VolumeMode::Earpiece)
    {
        setSpeakerVolume();
    }
    else
    {
        setEarpieceVolume();
    }
}

void Modem::enqueueCall(const String &number)
{
    if (_enqueuedCall.length() > 0)
    {
        Logger::warn("Call already enqueued!");
    }

    _enqueuedCall = number;
}

void Modem::call(const String &number)
{
    Logger::info("Dialing number: ");
    Logger::info(number);

    _modemImpl.callNumber(number);
    verifyCallState();
}

void Modem::hangUp()
{
    Logger::info("Hanging up...");

    _modemImpl.callHangup();
    verifyCallState();
}

void Modem::answer()
{
    Logger::info("Answering call...");

    _modemImpl.callAnswer();
    verifyCallState();
}

void Modem::verifyCallState()
{
    sendCommand("+CPAS");
}

bool Modem::messageAvailable() const
{
    return SerialAT.available() > 0;
}

StateResult Modem::deriveNewStateFromMessage(const AppState &currState)
{
    StateResult result = {currState, currState, "", "", true};

    if (!messageAvailable())
    {
        return result;
    }

    String msg = SerialAT.readStringUntil('\n');
    msg.trim();

    if (msg.length() == 0)
    {
        return result;
    }

    result.message = msg;

    Logger::info(String("Received from modem: ") + msg);

    if (msg == "OK" && currState == AppState::CheckHardware)
    {
        result.newState = AppState::CheckLine;
    }
    else if (msg.startsWith("+CGREG") && currState == AppState::CheckLine)
    {
        // Check if the modem is registered on the home network.
        int status = -1;
        int n = -1;

        sscanf(msg.c_str(), "+CGREG: %d,%d", &status, &n);

        if (status == 0 && n == 1)
        {
            result.newState = AppState::Idle;
        }
    }
    else if (msg.startsWith("+CLCC") && (currState == AppState::Idle || currState == AppState::IncomingCall || currState == AppState::IncomingCallRing || currState == AppState::InCall || currState == AppState::Dialing))
    {
        int callId = -1;
        int callDirection = -1;
        int callStatus = -1;
        int callMode = -1;
        int callMpty = -1;
        char callNumber[32] = {0};

        sscanf(msg.c_str(), "+CLCC: %d,%d,%d,%d,%d,\"%31[^\"]\"",
               &callId, &callDirection, &callStatus, &callMode, &callMpty, callNumber);

        char buffer[256];

        snprintf(buffer, sizeof(buffer),
                 "Call ID: %d, Call Direction: %d, Call Status: %d, Call Mode: %d, Call Mpty: %d, Call Number: %s",
                 callId, callDirection, callStatus, callMode, callMpty, callNumber);

        Logger::info(buffer);

        switch (callStatus)
        {
        case 0:
            // Call is active.
            result.newState = AppState::InCall;

            // TODO: Does this trigger when switching from call waiting to active call?
            _callState = CallState{callNumber, callDirection, {}};
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
            result.callerNumber = callNumber;
            break;
        case 5:
            // Call waiting.
            _callState.callWaitingDirection.push_back(callDirection);
            result.callWaiting = true;
            break;
        case 6:
            // Call is disconnected.
            // TODO: Handle case when call is disconnected and we have a call waiting.
            if (String(callNumber) == _callState.callerNumber)
            {
                result.newState = AppState::Idle;

                // TODO: What do we set here if we have a call waiting?
                _callState = CallState{"", -1, {}};
            }
            else if (_callState.callWaitingDirection.size() > 0)
            {
                // TODO: Does this happen automatically? Or do we need to switch to the call waiting?
                result.newState = AppState::InCall;
                _callState.callerNumber = _callState.callerNumber;
                _callState.callDirection = callDirection;
                _callState.callWaitingDirection.erase(std::remove(_callState.callWaitingDirection.begin(), _callState.callWaitingDirection.end(), callDirection), _callState.callWaitingDirection.end());
            }

            result.newState = AppState::Idle;
            _callState = CallState{"", -1, {}};
            break;
        default:
            Logger::warn(String("Unknown call status: ") + callStatus);
            break;
        }
    }
    else if (msg.startsWith("RING"))
    {
        if ((currState == AppState::IncomingCall || currState == AppState::Idle))
        {
            result.newState = AppState::IncomingCallRing;
        }
        else if (currState == AppState::IncomingCallRing)
        {
            result.newState = AppState::IncomingCall;
        }
    }
    else if (msg.startsWith("+CPAS") && (currState == AppState::IncomingCallRing || currState == AppState::IncomingCall || currState == AppState::InCall || currState == AppState::Dialing))
    {
        int callStatus = -1;

        sscanf(msg.c_str(), "+CPAS: %d", &callStatus);

        // TODO: What does this show when we have a call waiting?
        Logger::info(String("Call Status: ") + callStatus);

        switch (callStatus)
        {
        // TODO: Do we need to consider 6 as well?
        case 0:
            result.newState = AppState::Idle;
            break;
        case 4:
            result.newState = AppState::InCall;
            break;
        }
    }
    else
    {
        result.messageHandled = false;
    }

    return result;
}

void Modem::process(const StateResult &result)
{
    if (!_audioPlaying)
    {
        playPendingMp3();

        if (!_audioPlaying)
        {
            callPending();
        }
    }

    if (!result.messageHandled && result.message.length() > 0)
    {
        if (result.message.startsWith("+AUDIOSTATE: "))
        {
            if (result.message == "+AUDIOSTATE: audio play stop")
            {
                Logger::info("Audio stopped.");
                _audioPlaying = false;
            }
            else if (result.message == "+AUDIOSTATE: audio play")
            {
                Logger::info("Audio playing...");
                _audioPlaying = true;
            }
        }
        else if (result.message.startsWith("AT+STTONE=1"))
        {
            int toneId = -1;
            int duration = -1;

            sscanf(result.message.c_str(), "AT+STTONE=1,%d,%d", &toneId, &duration);
            _tonePlaying = true;

            Logger::info("Playing tone " + String(toneId) + " for " + String(duration) + "ms.");
        }
        else if (result.message == "+STTONE: 0")
        {
            Logger::info("Tone stopped.");
            _tonePlaying = false;
        }
        else
        {
            // TODO: Maybe put these in some collection/function.
            if (result.message != "OK" && result.message != "ATE0")
            {
                Logger::info("Unknown message: " + result.message);
            }
        }
    }

    if (result.newState == result.prevState)
    {
        return;
    }

    if (result.newState == AppState::InCall)
    {
        setEarpieceVolume();
    }

    if (result.newState == AppState::Idle)
    {
        setSpeakerVolume();
    }
}

void Modem::sendCommand(const String command)
{
    Logger::info("Sending command: AT" + command);

    _modemImpl.sendAT(command);
}

void Modem::sendCheckHardwareCommand()
{
    sendCommand("");
}

void Modem::sendCheckLineCommand()
{
    sendCommand("+CGREG?");
}

void Modem::playTone(const int toneId, const int duration)
{
    sendCommand(String("+STTONE=1,") + toneId + "," + duration);
}

void Modem::stopTone()
{
    sendCommand("+STTONE=0");
}

void Modem::playMp3(const MP3File &file, const int repeat)
{
    _audioPlaying = true;

    Logger::info("Playing MP3: " + String(file.fileName));

    if (file.fileName == nullptr)
    {
        _audioPlaying = false;
        Logger::error("Error: MP3File has a null fileName!");
        return;
    }

    char playCmd[128];
    snprintf(playCmd, sizeof(playCmd), "+CCMXPLAY=\"" MP3_DIR "/%s\",0,%d", file.fileName, repeat);
    sendCommand(playCmd);

    // if (_modemImpl.waitResponse() != 1)
    // {
    //     _audioPlaying = false;
    //     Logger::error("Play mp3 failed!");
    //     return;
    // }

    Logger::info("Playing MP3: " + String(file.fileName) + " success!");
}

void Modem::enqueueMp3(const MP3File &file, const int repeat)
{
    PendingMp3 pending = {file, repeat};
    _pendingMp3Queue.push(pending);
}

void Modem::playPendingMp3()
{
    if (_pendingMp3Queue.empty())
    {
        return;
    }

    Logger::info("Playing pending MP3...");

    PendingMp3 pending = _pendingMp3Queue.front();
    _pendingMp3Queue.pop();
    playMp3(pending.file, pending.repeat);
}

void Modem::callPending()
{
    if (_enqueuedCall.length() == 0)
    {
        return;
    }

    call(_enqueuedCall);
    _enqueuedCall = "";
}

void Modem::stopPlaying()
{
    if (!_audioPlaying)
    {
        return;
    }

    sendCommand("+CCMXSTOP");
}

void Modem::writeMp3s()
{
    Logger::info("Writing MP3s...");

    sendCommand("+FSMEM");

    if (_modemImpl.waitResponse("+FSMEM: C:(") != 1)
    {
        Logger::error("Failed to get memory size!");
        return;
    }

    String capSize = _modemImpl.stream.readStringUntil('\n');
    capSize.replace("\n", "");
    capSize.replace(")", "");

    Logger::info("Capacity [<total>/<used>]: " + capSize);

    sendCommand("+FSMKDIR=" MP3_DIR);
    _modemImpl.waitResponse(1000UL);
    sendCommand("+FSCD=" MP3_DIR);
    _modemImpl.waitResponse(1000UL);

    sendCommand("+FSLS");
    String fslsResponse;
    int response = _modemImpl.waitResponse(1000UL, fslsResponse);

    if (response == 1 && fslsResponse.startsWith("+FSLS:"))
    {
        String list;

        _modemImpl.waitResponse(1000UL, list);

        list.replace("OK", "");
        Logger::info("File list: " + list);
    }
    else
    {
        Logger::info("No files found - responses: " + String(response) + ", " + fslsResponse);
    }

    Logger::info("Writing audio files...");

    for (unsigned int i = 0; i < mp3FilesCount; i++)
    {
        char fullPath[64];
        snprintf(fullPath, sizeof(fullPath), MP3_DIR "/%s", mp3Files[i].fileName);

        Logger::info("Opening file: " + String(fullPath));

        String param = "+FSOPEN=";
        param += fullPath;
        sendCommand(param);

        int response = _modemImpl.waitResponse();
        if (response != 1)
        {
            Logger::error("Open file failed, response: " + String(response));
            continue;
        }

        Logger::info("Writing audio file: " + String(fullPath));

        String param2 = "+FSWRITE=1,";
        param2 += mp3Files[i].length;
        param2 += ",10";
        sendCommand(param2);

        if (_modemImpl.waitResponse(3000UL, "CONNECT") != 1)
        {
            Logger::error("Write file failed for: " + String(fullPath));
            continue;
        }

        _modemImpl.stream.write(mp3Files[i].data, mp3Files[i].length);

        if (_modemImpl.waitResponse(10000UL) == 1)
        {
            Logger::info("Write file " + String(fullPath) + " success!");
        }
        else
        {
            Logger::error("Write file failed for: " + String(fullPath));
        }

        sendCommand("+FSCLOSE=1");

        if (_modemImpl.waitResponse() != 1)
        {
            Logger::error("Close file failed for: " + String(fullPath));
            continue;
        }
    }
}