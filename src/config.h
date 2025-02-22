#pragma once

#define DEBUG 1
#define MP3_DIR "C:/mp3"
#define EARPIECE_VOLUME 2
#define SPEAKER_VOLUME 2
#define WRITE_MP3S 0

// Pin definitions:
#define RINGER_IN1_PIN 19
#define RINGER_IN2_PIN 18
#define RINGER_INH_PIN 5
#define HOOK_SWITCH_PIN 2
#define ROTARY_DIAL_IN_DIAL_PIN 0
#define ROTARY_DIAL_PULSE_PIN 15

// Manufacturer definitions:
#define SerialAT Serial1
#define BOARD_PWRKEY_PIN (4)
#define BOARD_LED_PIN (12)
#define MODEM_RING_PIN (13)
#define MODEM_DTR_PIN (14)
#define MODEM_RX_PIN (25)
#define MODEM_TX_PIN (26)
#define MODEM_RESET_PIN (27)
#define BOARD_POWERON_PIN (BOARD_LED_PIN)
#define MODEM_BAUDRATE (115200)
#define MODEM_RESET_LEVEL LOW
#define MODEM_GPS_ENABLE_GPIO (-1)
#define MODEM_GPS_ENABLE_LEVEL (-1)
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif