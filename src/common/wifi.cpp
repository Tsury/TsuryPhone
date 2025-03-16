#include "wifi.h"
#include "config.h"
#include "logger.h"

#ifdef WEB_SERIAL
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#endif

namespace {
  const constexpr int kWifiManagerPortalTimeout = 60 * 5;

#ifdef WEB_SERIAL
  const constexpr int kWebSerialPort = 32860;
  const constexpr int kWebSerialPrintInterval = 60000;
  const constexpr int kHttpOkStatus = 200;
#endif
}

#ifdef WEB_SERIAL
AsyncWebServer server(kWebSerialPort);
#endif

void Wifi::init() {
  Logger::infoln(F("Initializing WiFi..."));

  WiFi.mode(WIFI_STA);

  _wifiManager.setConfigPortalTimeout(kWifiManagerPortalTimeout);
  _wifiManager.setSaveConfigCallback([this]() { onWifiConnected(); });

  if (_wifiManager.autoConnect(kWifiSsid)) {
    onWifiConnected();
  } else {
    Logger::infoln(F("Config portal running"));
  }

  Logger::infoln(F("WiFi initialized!"));
}

#ifdef WEB_SERIAL
void Wifi::initWebSerial() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(kHttpOkStatus, F("text/plain"), WiFi.localIP().toString() + F("/webserial"));
  });

  WebSerial.begin(&server);

  WebSerial.onMessage([&](uint8_t *data, size_t len) {
    Logger::infoln(F("Received %lu bytes from WebSerial"), len);
    Serial.write(data, len);
    Serial.println();
    SerialAT.write(data, len);
    SerialAT.println();
    WebSerial.println(F("Received Data..."));

    for (size_t i = 0; i < len; i++) {
      WebSerial.print(char(data[i]));
    }

    WebSerial.println();
  });

  server.begin();
}
#endif

void Wifi::onWifiConnected() {
  Logger::infoln(F("Connected to WiFi"));
  Logger::infoln(F("IP Address: %s"), WiFi.localIP().toString().c_str());

#ifdef WEB_SERIAL
  initWebSerial();
#endif
}

void Wifi::process() {
  _wifiManager.process();

#ifdef WEB_SERIAL
  processWebSerial();
#endif
}

#ifdef WEB_SERIAL
void Wifi::processWebSerial() {
  if (millis() - _lastWebSerialPrint > kWebSerialPrintInterval) {
    WebSerial.print(F("IP address: "));
    WebSerial.println(WiFi.localIP());
    WebSerial.printf("Uptime: %lums\n", millis());
    // TODO: Consider implementing a free heap watchdog that will reset the device if the free heap
    // drops below a certain threshold.
    WebSerial.printf("Free heap: %u\n", ESP.getFreeHeap());
    _lastWebSerialPrint = millis();
  }

  WebSerial.loop();
}
#endif

void Wifi::openConfigPortal() {
  _wifiManager.setConfigPortalTimeout(kWifiManagerPortalTimeout);
  _wifiManager.startConfigPortal(kWifiSsid);
}