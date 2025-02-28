#pragma once

#include <Arduino.h>

// General configuration:
#define DEBUG 1
const constexpr char kMp3Dir[] = "C:/mp3";
const constexpr int kEarpieceVolume = 2;
const constexpr int kSpeakerVolume = 2;

// Pin definitions:
const constexpr int kRingerIn1Pin = 19;
const constexpr int kRingerIn2Pin = 18;
const constexpr int kRingerInhPin = 5;
const constexpr int kHookSwitchPin = 2;
const constexpr int kRotaryDialInDialPin = 0;
const constexpr int kRotaryDialPulsePin = 15;

// Manufacturer definitions:
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#define SerialAT Serial1

const constexpr int kBoardPowerKeyPin = 4;
const constexpr int kBoardLedPin = 12;
const constexpr int kModemRxPin = 25;
const constexpr int kModemTxPin = 26;
const constexpr int kModemResetPin = 27;
const constexpr int kBoardPowerOnPin = kBoardLedPin;
const constexpr int kModemBaudRate = 115200;
const constexpr int kModemResetLevel = LOW;
// const constexpr int MODEM_RING_PIN = 13;
// const constexpr int MODEM_DTR_PIN = 14;
// const constexpr int MODEM_GPS_ENABLE_GPIO = -1;
// const constexpr int MODEM_GPS_ENABLE_LEVEL = -1;