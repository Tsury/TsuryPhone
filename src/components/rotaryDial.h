#pragma once

#include "common/consts.h"
#include "common/state.h"
#include <Arduino.h>

const constexpr int kInvalidDialedDigit = 99;

class RotaryDial {
public:
  void init() const;
  void process(State &state);

  int getDialedDigit() const;

private:
  int _inDialState = HIGH;
  int _inDialPreviousState = HIGH;
  int _pulseState = HIGH;
  int _pulsePreviousState = HIGH;
  int _counter = 0;

  uint32_t _inDialChangeTime = 0UL;
  uint32_t _pulseChangeTime = 0UL;

  char _dialedDigit = kInvalidDialedDigit;
};
