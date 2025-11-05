#pragma once

#include "consts.h"
#include <Arduino.h>
#include <cstring>
#include <utility>

enum class AppState {
  Startup,
  CheckHardware,
  CheckLine,
  Idle,
  InvalidNumber,
  IncomingCall,
  IncomingCallRing,
  InCall,
  Dialing,
};

enum class VolumeMode {
  Earpiece,
  Speaker,
};

struct CallLeg {
  int id;
  bool isPriority;
  bool isBlocked;
  bool isOnHold;
  bool shouldReject;
  bool introducedCaller;
  bool rangAtLeastOnce;
  bool otherPartyDropped;
  bool isIncoming;
  bool isMuted;
  unsigned long startedAtMs;
  char number[kSmallBufferSize];

  CallLeg() {
    reset();
  }

  void reset() {
    id = -1;
    isPriority = false;
    isBlocked = false;
    isOnHold = false;
    shouldReject = false;
    introducedCaller = false;
    rangAtLeastOnce = false;
    otherPartyDropped = false;
    isIncoming = false;
    isMuted = false;
    startedAtMs = 0UL;
    number[0] = '\0';
  }

  void setNumber(const char *value) {
    if (value == nullptr) {
      number[0] = '\0';
      return;
    }
    std::strncpy(number, value, kSmallBufferSize - 1);
    number[kSmallBufferSize - 1] = '\0';
  }

  bool isValid() const {
    return id != -1;
  }
};

struct CallState {
  CallLeg active;
  CallLeg waiting;
  int waitingReleaseId;
  bool playedCallWaitingTone;

  CallState() : active(), waiting(), waitingReleaseId(-1), playedCallWaitingTone(false) {}

  void reset() {
    active.reset();
    waiting.reset();
    waitingReleaseId = -1;
    playedCallWaitingTone = false;
  }

  bool hasCallWaiting() const {
    return waiting.isValid();
  }

  void clearWaiting() {
    waiting.reset();
    waitingReleaseId = -1;
  }

  void promoteWaitingToActive(unsigned long now) {
    if (!hasCallWaiting()) {
      return;
    }
    std::swap(active, waiting);
    active.isOnHold = false;
    active.shouldReject = false;
    if (active.startedAtMs == 0UL) {
      active.startedAtMs = now;
    }
    waiting.isOnHold = true;
    waiting.shouldReject = false;
    waitingReleaseId = -1;
  }
};

struct State {
  AppState newAppState;
  AppState prevAppState;
  CallState callState;
  char lastModemMessage[kBigBufferSize];
  bool messageHandled;
  bool isDnd;
  bool isMaintenanceMode;
  bool isHookOff;
  char currentDialingNumber[kSmallBufferSize]; // Current number being dialed
  VolumeMode volumeMode;

  State()
      : newAppState(AppState::Startup),
        prevAppState(AppState::Startup),
        callState(),
        messageHandled(false),
        isDnd(false),
        isMaintenanceMode(false),
        isHookOff(false),
        volumeMode(VolumeMode::Earpiece) {
    lastModemMessage[0] = '\0';
    currentDialingNumber[0] = '\0';
  }
};

const __FlashStringHelper *appStateToString(const AppState state);
const __FlashStringHelper *volumeModeToString(const VolumeMode mode);
