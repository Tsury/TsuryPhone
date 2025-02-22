#pragma once

#include <queue>
#include <Arduino.h>
#include "config.h"
#include "common/common.h"
#include "generated/mp3.h"
#include <TinyGsmClient.h>

struct CallState
{
    String callerNumber;
    int callDirection;
    std::vector<int> callWaitingDirection;
};

struct StateResult
{
    AppState newState;
    AppState prevState;
    String callerNumber;
    String message;
    bool messageHandled;
    bool callWaiting;
};

enum class VolumeMode
{
    Earpiece,
    Speaker
};

struct PendingMp3
{
    MP3File file;
    int repeat;
};

class Modem
{
public:
    Modem();

    void init();
    void process(const StateResult &result);

    StateResult deriveNewStateFromMessage(const AppState &currState);

    void enqueueCall(const String &number);
    void hangUp();
    void answer();

    void sendCommand(const String command);

    void playTone(const int toneId, const int duration);
    void stopTone();

    void sendCheckHardwareCommand();
    void sendCheckLineCommand();

    void toggleVolume();
    void setEarpieceVolume();
    void setSpeakerVolume();

    void enqueueMp3(const MP3File &file, const int repeat = 0);
    void stopPlaying();

private:
    void playMp3(const MP3File &file, const int repeat = 0);
    void writeMp3s();
    void playPendingMp3();

    void callPending();
    void call(const String &number);
    void verifyCallState();

    void enableHangUp();
    void disableEcho();

    void setVolume(const int volume);

    bool messageAvailable() const;

    std::queue<PendingMp3> _pendingMp3Queue;

    VolumeMode _volumeMode = VolumeMode::Earpiece;

    TinyGsm _modemImpl;

    String _enqueuedCall;

    CallState _callState;

    bool _audioPlaying = false;
    bool _tonePlaying = false;
    bool _lastTimeCheckedLine = false;
};
