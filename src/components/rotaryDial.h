#pragma once

#include "common/common.h"
#include <Arduino.h>

struct DialedNumberResult {
  char callerNumber[kSmallBufferSize];
  int dialedDigit;
};

class RotaryDial {
public:
  void init() const;
  void process();

  void resetCurrentNumber();
  int getDialedDigit() const;
  DialedNumberResult getCurrentNumber();

private:
  int _inDialState = HIGH;
  int _inDialPreviousState = HIGH;
  int _pulseState = HIGH;
  int _pulsePreviousState = HIGH;
  int _counter = 0;

  unsigned long _inDialChangeTime = 0;
  unsigned long _pulseChangeTime = 0;

  char _dialedDigit = 99;
  char _currentNumber[kSmallBufferSize] = "";
};
