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
  int inDialState = HIGH;
  int inDialPreviousState = HIGH;
  int pulseState = HIGH;
  int pulsePreviousState = HIGH;
  int counter = 0;

  unsigned long inDialChangeTime = 0;
  unsigned long pulseChangeTime = 0;

  char _dialedDigit = 99;
  char currentNumber[kSmallBufferSize] = "";
};
