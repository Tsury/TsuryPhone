#pragma once

#include "common/consts.h"
#include <Arduino.h>

const constexpr int kInvalidDialedDigit = 99;

struct DialedNumberResult {
  char callNumber[kSmallBufferSize];
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

  uint32_t _inDialChangeTime = 0UL;
  uint32_t _pulseChangeTime = 0UL;

  char _dialedDigit = kInvalidDialedDigit;
  char _currentNumber[kSmallBufferSize] = "";
};
