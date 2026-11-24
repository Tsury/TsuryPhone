#pragma once

#include "common/ringBuffer.h"
#include "common/state.h"
#include "config.h"
#include "generated/mp3.h"
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <queue>

// Forward declaration
class DeviceConfig;

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

// SMS storage commands. Banks are modem storage names such as "SM" or "ME".
enum class SmsOpType : uint8_t { List, Read, Delete };

struct SmsOp {
  SmsOpType type = SmsOpType::List;
  char bank[4] = "";
  int index = -1;
};

// Receives SMS lines from the UART owner. SMS lines never reach logs or State.
class SmsModemListener {
public:
  virtual ~SmsModemListener() = default;
  // +CMTI: a new message was stored at bank/index.
  virtual void onSmsStored(const char *bank, int index) = 0;
  // A received-message PDU from AT+CMGR or AT+CMGL, or from +CMT (index -1, not stored).
  virtual void onSmsPdu(const char *bank, int index, int tpduLength, const char *pduHex) = 0;
  virtual void onSmsOpDone(const SmsOp &op, bool ok) = 0;
};

class Modem {
public:
  Modem(DeviceConfig &config);

  // SMS commands run one at a time, only while the phone is idle and the UART is quiet.
  void setSmsListener(SmsModemListener *listener) {
    _smsListener = listener;
  }
  bool enqueueSmsOp(const SmsOp &op);
  // Increments on every modem (re)initialization; storage must be listed again after it.
  uint32_t initCount() const {
    return _initCount;
  }

  void init();
  void process(State &state);

  void deriveStateFromMessage(State &currState);

  void enqueueCall(const char *number);
  void hangUp();
  void answer();
  void switchToCallWaiting();
  void rejectCallWaiting(CallState &callState);

  // DTMF tone generation during active calls (integration-only, not triggered by rotary dial)
  // Sends both local audio feedback (AT+CLDTMF) and remote DTMF signal (AT+VTS)
  bool sendDTMFTone(char digit);

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
  void setVolume(const int volume);
  void setMicGain(const int gain);
  VolumeMode getCurrentVolumeMode() const {
    return _volumeMode;
  }

  void toggleMute();
  void mute();
  void unmute();
  bool isMuted() const {
    return _isMuted;
  }

private:
  void initModem();
  void hardResetModem();
  bool probeOK(uint32_t timeoutMs);

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
  void enableSmsIndications();
  void disableUnneededFeatures();
  void disableUnneededFeaturesAfterInit();
  void clearCallWaitingState(CallState &callState);

  bool messageAvailable() const;
  bool isKnownMessage(const char *msg) const;

  bool handleSmsLine(const char *line);
  void processSmsOps(const State &state);
  void sendSmsOpCommand();
  void finishSmsOp(bool ok);

  void keepAliveWatchdog();
  void sendKeepAlive();
  void reset();

  RingBuffer<AudioItem, 10> _audioQueue;

  VolumeMode _volumeMode = VolumeMode::Earpiece;
  TinyGsm _modemImpl;

  char _enqueuedCall[kSmallBufferSize] = "";

  bool _isPlayingAudio = false;
  bool _lastTimeCheckedLine = false;
  bool _waitingForKeepAlive = false;
  bool _isMuted = false;

  uint32_t _lastAudioStopMillis = 0UL;
  uint32_t _lastKeepAliveSent = 0UL;
  uint32_t _watchdogResetCounter = 0;
  uint8_t _keepAliveRetryCount = 0;

  SmsModemListener *_smsListener = nullptr;
  RingBuffer<SmsOp, 16> _smsOps;
  SmsOp _smsOp;
  bool _smsOpActive = false;
  bool _smsSelectingBank = false;
  uint32_t _smsOpSentMs = 0UL;
  char _smsBank[4] = "";
  // Set by a +CMGR/+CMGL/+CMT header: the next line is that message's PDU.
  bool _smsPduExpected = false;
  char _smsPduBank[4] = "";
  int _smsPduIndex = -1;
  int _smsPduLength = -1;
  uint32_t _lastCommandMs = 0UL;
  uint32_t _lastLineMs = 0UL;
  uint32_t _initCount = 0;

  DeviceConfig &_config;
};
