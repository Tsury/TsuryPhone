#include "wifi.h"
#include "config.h"
#include "logger.h"

#ifdef WEB_SERIAL
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#endif

namespace {
  const constexpr int kWifiManagerPortalTimeoutSeconds = 60 * 5;
  const constexpr uint32_t kWifiManagerPortalTimeoutMs = kWifiManagerPortalTimeoutSeconds * 1000UL;

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

  // Set non-blocking mode and disable auto timeout for manual management
  _wifiManager.setConfigPortalBlocking(false);
  _wifiManager.setConfigPortalTimeout(0); // Disable auto timeout, we'll manage it manually
  _wifiManager.setSaveConfigCallback([this]() { onWifiConnected(); });

  if (_wifiManager.autoConnect(getWifiSsid().c_str())) {
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

  // Close config portal if it was active
  if (_configPortalActive) {
    closeConfigPortal();
  }

#ifdef WEB_SERIAL
  initWebSerial();
#endif
}

void Wifi::process() {
  _wifiManager.process();
  processConfigPortal();

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

void Wifi::processConfigPortal() {
  // Handle config portal open request
  if (_configPortalOpenRequested && !_configPortalActive) {
    _configPortalActive = true;
    _configPortalOpenRequested = false;
    _configPortalStartTime = millis();
    _configPortalTimeout = kWifiManagerPortalTimeoutMs;

    _wifiManager.setConfigPortalBlocking(false);
    _wifiManager.startConfigPortal(getWifiSsid().c_str());
  }

  // Handle config portal close request
  if (_configPortalCloseRequested && _configPortalActive) {
    _configPortalActive = false;
    _configPortalCloseRequested = false;
    _configPortalStartTime = 0UL;
    _configPortalTimeout = 0UL;
    _wifiManager.stopConfigPortal();
  }

  // Check if config portal timeout has been reached
  if (_configPortalActive && _configPortalTimeout > 0) {
    uint32_t elapsed = millis() - _configPortalStartTime;
    if (elapsed >= _configPortalTimeout) {
      Logger::infoln(F("Config portal timeout reached, closing portal"));
      _configPortalCloseRequested = true; // Request close via process loop
      onConfigPortalTimeout();
    }
  }
}

void Wifi::openConfigPortal() {
  Logger::infoln(F("Requesting config portal open"));
  _configPortalOpenRequested = true;
}

void Wifi::closeConfigPortal() {
  Logger::infoln(F("Requesting config portal close"));
  _configPortalCloseRequested = true;
}

bool Wifi::isConfigPortalActive() {
  return _configPortalActive;
}

void Wifi::setConfigPortalTimeoutCallback(std::function<void()> callback) {
  _configPortalTimeoutCallback = callback;
  // Note: We don't set the WiFiManager callback since we manage timeout manually
}

void Wifi::onConfigPortalTimeout() {
  if (_configPortalTimeoutCallback) {
    _configPortalTimeoutCallback();
  }
}