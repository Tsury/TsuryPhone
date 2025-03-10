#pragma once

#include "common/ringBuffer.h"
#include "common/state.h"
#include "config.h"
#include "generated/mp3.h"
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <queue>

// As per SIMCom's A76XX Series AT Command Manual
enum class Tone {
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

enum class VolumeMode { Earpiece, Speaker };

enum class AudioType { Tone, Mp3 };

struct AudioItem {
  AudioType type;
  Tone toneId;
  int toneDuration;
  const char *filename;
  int repeat;
};

struct PendingMp3 {
  PendingMp3() : filename(nullptr), repeat(0) {}
  PendingMp3(const char *filename, const int repeat = 0) : filename(filename), repeat(repeat) {}

  const char *filename;
  int repeat;
};

class Modem {
public:
  Modem();

  void init();
  void process(const State &state);

  void deriveStateFromMessage(State &currState);

  void enqueueCall(const char *number);
  void hangUp();
  void answer();
  void switchToCallWaiting();

  void enqueueTone(const Tone toneId, const int duration);
  void stopTone();
  void enqueueMp3(const char *file, const int repeat = 0);
  void stopMp3();
  void stopAllAudio();

  void sendCheckHardwareCommand();
  void sendCheckLineCommand();

  void toggleVolume();
  void setEarpieceVolume();
  void setSpeakerVolume();

private:
  void startModem();

  template <typename... Args> void sendCommand(const __FlashStringHelper *command, Args... args);
  void sendCommand(const StringSumHelper &command);
  void sendCommand(const __FlashStringHelper *command);
  void sendCommand(const char *command);

  void playMp3(const char *fileName, const int repeat = 0);
  void playTone(const Tone toneId, const int duration);
  bool hasAudioToPlay();
  void playNextAudioItem();

  void callPending();
  void call(const char *number);
  void verifyCallState();

  void enableHangUp();
  void disableUnneededFeatures();

  void setVolume(const int volume);
  void setMicGain(const int gain);

  bool messageAvailable() const;
  bool isKnownMessage(const char *msg) const;

  void keepAliveWatchdog();
  void sendKeepAlive();
  void resetModem();

  RingBuffer<AudioItem, 10> _audioQueue;

  VolumeMode _volumeMode = VolumeMode::Earpiece;
  TinyGsm _modemImpl;

  char _enqueuedCall[kSmallBufferSize] = "";

  bool _isPlayingAudio = false;
  bool _lastTimeCheckedLine = false;
  bool _keepAlivePending = false;

  uint32_t _lastAudioStopMillis = 0UL;
  uint32_t _lastKeepAliveSent = 0UL;
};
