#include "wifi.h"
#include "config.h"
#include "logger.h"
#include <cstdio>
#include <time.h>

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

Wifi::Wifi(DeviceConfig &config) : _config(config) {}

void Wifi::init() {
  Logger::infoln(F("Initializing WiFi..."));

  WiFi.mode(WIFI_STA);

  // Configure manual portal management
  _wifiManager.setConfigPortalBlocking(false);
  _wifiManager.setConfigPortalTimeout(0); // Disable auto timeout, we'll manage it manually
  _wifiManager.setSaveConfigCallback([this]() { onWifiConnected(); });

  if (_wifiManager.autoConnect(_config.getWifiSsid().c_str())) {
    onWifiConnected();
  } else {
    Logger::infoln(F("Config portal running"));
    startConfigPortalSession(kWifiManagerPortalTimeoutMs);
  }

  Logger::infoln(F("WiFi initialized!"));
}

#ifdef WEB_SERIAL
void Wifi::initWebSerial() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(kHttpOkStatus, F("text/plain"), WiFi.localIP().toString() + F("/webserial"));
  });

  WebSerial.begin(&server);
  _webSerialActive = true;

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
  if (!_webSerialActive) {
    initWebSerial();
  }
#endif
}

void Wifi::process(State &state) {
  _wifiManager.process();
  processConfigPortal();

#ifdef WEB_SERIAL
  processWebSerial(state);
#endif
}

#ifdef WEB_SERIAL
void Wifi::processWebSerial(State &state) {
  if (!_webSerialActive || WiFi.status() != WL_CONNECTED) {
    return;
  }

  const unsigned long now = millis();

  if (now - _lastWebSerialPrint > kWebSerialPrintInterval) {
    const unsigned long uptimeMs = now;
    const uint32_t freeHeap = ESP.getFreeHeap();
    struct tm timeinfo;
    char timeDisplay[6] = "N/A";

    if (getLocalTime(&timeinfo)) {
      snprintf(timeDisplay, sizeof(timeDisplay), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }

    Logger::infoln(F("Runtime stats: uptime=%lums freeHeap=%u currentTime=%s DND=%s"),
                   uptimeMs,
                   freeHeap,
                   timeDisplay,
                   state.isDnd ? "true" : "false");

    _lastWebSerialPrint = now;
  }

  WebSerial.loop();
}
#endif

void Wifi::startConfigPortalSession(uint32_t timeoutMs) {
  WiFi.mode(WIFI_AP_STA);
  _configPortalActive = true;
  _configPortalCloseRequested = false;
  _configPortalStartTime = millis();
  _configPortalTimeout = timeoutMs;
  _wifiManager.startConfigPortal(_config.getWifiSsid().c_str());
}

void Wifi::onConfigPortalClosed() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

void Wifi::processConfigPortal() {
  // Handle config portal open request
  if (_configPortalOpenRequested && !_configPortalActive) {
    _configPortalOpenRequested = false;
    startConfigPortalSession(kWifiManagerPortalTimeoutMs);
  }

  // Handle config portal close request
  if (_configPortalCloseRequested && _configPortalActive) {
    _configPortalActive = false;
    _configPortalCloseRequested = false;
    _configPortalStartTime = 0UL;
    _configPortalTimeout = 0UL;
    _wifiManager.stopConfigPortal();
    onConfigPortalClosed();
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

void Wifi::resetCredentials() {
  Logger::warnln(F("Clearing stored WiFi credentials"));
  _wifiManager.resetSettings();
  WiFi.disconnect(true, true);
}

void Wifi::onConfigPortalTimeout() {
  // When portal times out, notify to exit maintenance mode
  if (_portalTimeoutCallback) {
    _portalTimeoutCallback();
  }
}