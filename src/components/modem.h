#pragma once

#include <queue>
#include <Arduino.h>
#include "config.h"
#include "common/common.h"
#include "common/ringBuffer.h"
#include "generated/mp3.h"
#include <TinyGsmClient.h>

// As per SIMCom's A76XX Series AT Command Manual
enum class Tone
{
    DialTone = 1,
    CalledSubscriberBusy = 2,
    Congestion = 3,
    RadioPathAcknowledge = 4,
    RadioPathNotAvailableOrCallDropped = 5,
    ErrorOrSpecialInformation = 6,
    CallWaitingTone = 7,
    RingingTone = 8,
    GeneralBeep = 16,
    PositiveAcknowledgeTone = 17,
    NegativeAcknowledgeOrErrorTone = 18,
    IndianDialTone = 19,
    AmericanDialTone = 20,
};

struct CallState
{
    int callId;
    int callWaitingId;
    bool isCallWaitingOnHold = false;
    char callerNumber[kSmallBufferSize];

    CallState() : callId(-1), callWaitingId(-1), isCallWaitingOnHold(false), callerNumber("")
    {
    }

    CallState(const char *number, const int callId, const int callWaitingId = -1, const bool callWaitingOnHold = false)
        : callId(callId), callWaitingId(callWaitingId), isCallWaitingOnHold(callWaitingOnHold)
    {
        strncpy(callerNumber, number, kSmallBufferSize - 1);
        callerNumber[kSmallBufferSize - 1] = '\0';
    }
};

struct StateResult
{
    AppState newState;
    AppState prevState;
    char callerNumber[kSmallBufferSize];
    char message[kBigBufferSize];
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
    PendingMp3() : filename(nullptr), repeat(0) {}
    PendingMp3(const char *filename, const int repeat = 0) : filename(filename), repeat(repeat) {}

    const char *filename;
    int repeat;
};

class Modem
{
public:
    Modem();

    void init();
    void process(const StateResult &result);

    StateResult deriveNewStateFromMessage(const AppState &currState);

    void enqueueCall(const char *number);
    void hangUp();
    void answer();
    void switchToCallWaiting();

    template <typename... Args>
    void sendCommand(const __FlashStringHelper *command, Args... args);
    void sendCommand(const StringSumHelper &command);
    void sendCommand(const __FlashStringHelper *command);
    void sendCommand(const char *command);

    void playTone(const int toneId, const int duration);
    void stopTone();

    void sendCheckHardwareCommand();
    void sendCheckLineCommand();

    void toggleVolume();
    void setEarpieceVolume();
    void setSpeakerVolume();

    void enqueueMp3(const char *file, const int repeat = 0);
    void stopPlaying();

private:
    void playMp3(const char *file, const int repeat = 0);
    void playPendingMp3();

    void callPending();
    void call(const char *number);
    void verifyCallState();

    void enableHangUp();
    void disableUnneededFeatures();

    void setVolume(const int volume);

    bool messageAvailable() const;

    RingBuffer<PendingMp3, 10> _pendingMp3Queue = RingBuffer<PendingMp3, 10>();

    VolumeMode _volumeMode = VolumeMode::Earpiece;

    TinyGsm _modemImpl;

    char _enqueuedCall[kSmallBufferSize] = "";

    CallState _callState;

    bool _audioPlaying = false;
    bool _tonePlaying = false;
    bool _lastTimeCheckedLine = false;
};
