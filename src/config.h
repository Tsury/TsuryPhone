#pragma once

#include <Arduino.h>

// General configuration:
const constexpr char kMp3Dir[] = "C:/mp3";
const constexpr char *kWifiWebPortalNumber = "3123";
const constexpr char *kResetNumber = "5555";

// System numbers collection for easier maintenance
const constexpr char *kSystemNumbers[] = {kResetNumber, kWifiWebPortalNumber};
const constexpr size_t kSystemNumbersCount = sizeof(kSystemNumbers) / sizeof(kSystemNumbers[0]);

const constexpr char *timeZone = "IST-2IDT,M3.4.4/26,M10.5.0";

// Utility function to generate device ID from chip MAC
inline String generateDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  String deviceId = String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  deviceId.toUpperCase();
  return deviceId;
}

// Utility function to generate device name from device ID
inline String generateDeviceName(const String &deviceId) {
  return "TsuryPhone-" + deviceId.substring(deviceId.length() - 4);
}

inline String getDeviceName() {
  String deviceId = generateDeviceId();
  return generateDeviceName(deviceId);
}

inline String getWifiSsid() {
  return getDeviceName();
}

// Default audio settings (used for fallback)
const constexpr int kEarpieceVolume = 2;
const constexpr int kEarpieceMicGain = 7;
const constexpr int kSpeakerVolume = 7;
const constexpr int kSpeakerMicGain = 7;
const constexpr int kDndStartHour = 18;
const constexpr int kDndStartMinute = 30;
const constexpr int kDndEndHour = 8;
const constexpr int kDndEndMinute = 30;
const constexpr bool kDndForce = false;
const constexpr bool kDndScheduled = true;

// Pin definitions:
const constexpr int kRingerIn1Pin = 33;
const constexpr int kRingerIn2Pin = 32;
// The LilyGO-T-A7670E v1.0 uses pin 14 for DTR, but we don't use it.
const constexpr int kRingerInhPin = 14;
const constexpr int kHookSwitchPin = 0;
const constexpr int kRotaryDialInDialPin = 2;
const constexpr int kRotaryDialPulsePin = 15;

// Manufacturer definitions:
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#define SerialAT Serial1
#define BOARD_LED_PIN 12
#define BOARD_POWERON_PIN BOARD_LED_PIN

const constexpr int kBoardPowerKeyPin = 4;
const constexpr int kModemRxPin = 25;
const constexpr int kModemTxPin = 26;
const constexpr int kModemResetPin = 27;
const constexpr int kModemBaudRate = 115200;
const constexpr int kModemResetLevel = LOW;

// Unused manufacturer definitions:
// const constexpr int MODEM_RING_PIN = 13;
// const constexpr int MODEM_DTR_PIN = 14;
// const constexpr int MODEM_GPS_ENABLE_GPIO = -1;
// const constexpr int MODEM_GPS_ENABLE_LEVEL = -1;