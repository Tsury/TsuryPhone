#pragma once

#include <Arduino.h>

// General configuration:
#define DEBUG 1
constexpr char kMp3Dir[] = "C:/mp3";
constexpr int kEarpieceVolume = 2;
constexpr int kSpeakerVolume = 2;

// Pin definitions:
constexpr int kRingerIn1Pin = 19;
constexpr int kRingerIn2Pin = 18;
constexpr int kRingerInhPin = 5;
constexpr int kHookSwitchPin = 2;
constexpr int kRotaryDialInDialPin = 0;
constexpr int kRotaryDialPulsePin = 15;

// Manufacturer definitions:
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#define SerialAT Serial1

constexpr int kBoardPowerKeyPin = 4;
constexpr int kBoardLedPin = 12;
constexpr int kModemRxPin = 25;
constexpr int kModemTxPin = 26;
constexpr int kModemResetPin = 27;
constexpr int kBoardPowerOnPin = kBoardLedPin;
constexpr int kModemBaudRate = 115200;
constexpr int kModemResetLevel = LOW;
// constexpr int MODEM_RING_PIN = 13;
// constexpr int MODEM_DTR_PIN = 14;
// constexpr int MODEM_GPS_ENABLE_GPIO = -1;
// constexpr int MODEM_GPS_ENABLE_LEVEL = -1;