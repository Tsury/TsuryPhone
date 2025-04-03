#include "../src/common/logger.cpp"
#include "../src/common/logger.h"
#include "../src/config.h"
#include "Arduino.h"
#include "generated/writeMp3.h"
#include <TinyGsmClient.h>

#define MODEM_RESET_DELAY 1500
#define GENERIC_DELAY 50
#define MP3_DIR "C:/mp3"

TinyGsm _modemImpl = TinyGsm(SerialAT);

void sendCommand(const char *command) {
  Logger::infoln(F("Sending command: AT%s"), command);
  _modemImpl.sendAT(command);
}

void sendCommand(const __FlashStringHelper *command) {
  Logger::infoln(F("Sending command: AT%s"), command);
  _modemImpl.sendAT(command);
}

// TODO: Add a way to delete files from the modem.
void writeMp3s() {
  Logger::infoln(F("Writing MP3s..."));

  sendCommand(F("+FSMEM"));

  if (_modemImpl.waitResponse("+FSMEM: C:(") != 1) {
    Logger::errorln(F("Failed to get memory size!"));
    return;
  }

  String capSize = _modemImpl.stream.readStringUntil('\n');
  capSize.replace(F("\n"), F(""));
  capSize.replace(F(")"), F(""));

  Logger::infoln(F("Capacity [<total>/<used>]: %s"), capSize.c_str());

  sendCommand(F("+FSMKDIR=" MP3_DIR));
  _modemImpl.waitResponse(1000UL);

  sendCommand(F("+FSCD=" MP3_DIR));
  _modemImpl.waitResponse(1000UL);
  _modemImpl.waitResponse(1000UL);
  _modemImpl.waitResponse(1000UL);

  sendCommand(F("+FSLS"));

  String fslsResponse;
  int response = _modemImpl.waitResponse(5000UL, fslsResponse);
  if (response == 1) {
    if (fslsResponse.startsWith(F("AT+FSLS"))) {
      // TODO: Not sure whether it's waitResponse's fault, but the list returned is truncated if too
      // long This is not the case when using SerialAT directly.
      Logger::infoln(F("File list: %s"), fslsResponse.c_str());
    } else {
      Logger::errorln(F("Unexpected file list response: %s"), fslsResponse.c_str());
    }
  } else {
    Logger::infoln(F("No files found - responses: %d, %s"), response, fslsResponse);
  }

  Logger::infoln(F("Writing audio files..."));

  for (unsigned int i = 0; i < mp3FilesCount; i++) {
    char fullPath[64];
    snprintf(fullPath, sizeof(fullPath), MP3_DIR "/%s", mp3Files[i].fileName);

    Logger::infoln(F("Opening file: %s"), fullPath);

    char param[72];
    snprintf(param, sizeof(param), "+FSOPEN=%s", fullPath);
    sendCommand(param);

    int response = _modemImpl.waitResponse();
    if (response != 1) {
      Logger::errorln(F("Open file failed, response: %d"), response);
      continue;
    }

    Logger::infoln(F("Writing audio file: %s"), fullPath);

    char param2[20];
    snprintf(param2, sizeof(param2), "+FSWRITE=1,%u,10", mp3Files[i].length);
    sendCommand(param2);

    if (_modemImpl.waitResponse(3000UL, "CONNECT") != 1) {
      Logger::errorln(F("Write file failed for: %s"), fullPath);
      continue;
    }

    _modemImpl.stream.write(mp3Files[i].data, mp3Files[i].length);

    if (_modemImpl.waitResponse(10000UL) == 1) {
      Logger::infoln(F("Write file %s success!"), fullPath);
    } else {
      Logger::errorln(F("Write file failed for: %s"), fullPath);
    }

    sendCommand(F("+FSCLOSE=1"));

    if (_modemImpl.waitResponse() != 1) {
      Logger::errorln(F("Close file failed for: %s"), fullPath);
      continue;
    }
  }

  Logger::infoln(F("Done writing audio files!"));
}

void setup() {
  Serial.begin(115200);

  Logger::infoln(F("TsuryPhone starting..."));

  SerialAT.begin(kModemBaudRate, SERIAL_8N1, kModemRxPin, kModemTxPin);

#ifdef BOARD_POWERON_PIN
  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

  pinMode(kModemResetPin, OUTPUT);
  digitalWrite(kModemResetPin, kModemResetLevel);
  delay(MODEM_RESET_DELAY);
  digitalWrite(kModemResetPin, !kModemResetLevel);

  pinMode(kBoardPowerKeyPin, OUTPUT);
  digitalWrite(kBoardPowerKeyPin, HIGH);
  delay(GENERIC_DELAY);
  digitalWrite(kBoardPowerKeyPin, LOW);

  while (!_modemImpl.testAT(GENERIC_DELAY)) {
    delay(GENERIC_DELAY);
  }

  writeMp3s();
}

void loop() {
  if (SerialAT.available()) {
    Serial.write(SerialAT.read());
  }
  if (Serial.available()) {
    SerialAT.write(Serial.read());
  }
}