#pragma once

#include "consts.h"
#include <Arduino.h>

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

struct CallState {
  int callId;
  int callWaitingId;
  bool isCallWaitingOnHold = false;
  bool introducedCaller = false;
  bool playedCallWaitingTone = false;
  bool rangAtLeastOnce = false;
  bool otherPartyDropped = false;
  bool isPriority = false;
  char callNumber[kSmallBufferSize];

  CallState()
      : callId(-1),
        callWaitingId(-1),
        isCallWaitingOnHold(false),
        introducedCaller(false),
        playedCallWaitingTone(false),
        rangAtLeastOnce(false),
        otherPartyDropped(false),
        isPriority(false) {
    callNumber[0] = '\0';
  }

  void setcallNumber(const char *number) {
    strncpy(callNumber, number, kSmallBufferSize - 1);
    callNumber[kSmallBufferSize - 1] = '\0';
  }

  bool hasCallWaiting() const {
    return callWaitingId != -1;
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
  char currentDialingNumber[kSmallBufferSize]; // Current number being dialed

  State()
      : newAppState(AppState::Startup),
        prevAppState(AppState::Startup),
        callState(),
        messageHandled(false),
        isDnd(false),
        isMaintenanceMode(false) {
    lastModemMessage[0] = '\0';
    currentDialingNumber[0] = '\0';
  }
};

const __FlashStringHelper *appStateToString(const AppState state);
