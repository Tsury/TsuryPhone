#include "wifi.h"
#include "config.h"
#include "logger.h"

#ifdef WEB_SERIAL
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#endif

namespace {
  const constexpr int kWifiManagerHttpPort = 8080;
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

String Wifi::generateDeviceId() {
  if (_deviceId.length() == 0) {
    // Get the last 3 bytes of MAC address for device ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char deviceIdBuffer[7];
    sprintf(deviceIdBuffer, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    _deviceId = String(deviceIdBuffer);
  }
  return _deviceId;
}

String Wifi::generateWifiSsid() {
  if (_wifiSsid.length() == 0) {
    _wifiSsid = String(kWifiSsidBase) + "-" + generateDeviceId();
  }
  return _wifiSsid;
}

void Wifi::init() {
  Logger::infoln(F("Initializing WiFi..."));

  WiFi.mode(WIFI_STA);

  // Set hostname to TsuryPhone-[ID]
  String hostname = "TsuryPhone-" + generateDeviceId();
  WiFi.setHostname(hostname.c_str());
  _wifiManager.setHostname(hostname.c_str());
  _wifiManager.setConfigPortalTimeout(kWifiManagerPortalTimeout);
  _wifiManager.setSaveConfigCallback([this]() { onWifiConnected(); });

  String wifiSsid = generateWifiSsid();
  if (_wifiManager.autoConnect(wifiSsid.c_str())) {
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
  // Handle async config portal request
  if (_configPortalRequested && !_configPortalActive) {
    _configPortalRequested = false;
    openConfigPortal();
  }

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
  _configPortalActive = true;
  _wifiManager.setHttpPort(kWifiManagerHttpPort);
  _wifiManager.setConfigPortalTimeout(kWifiManagerPortalTimeout);
  String wifiSsid = generateWifiSsid();
  _wifiManager.startConfigPortal(wifiSsid.c_str());
  _configPortalActive = false;
}

void Wifi::openConfigPortalAsync() {
  if (!_configPortalActive) {
    _configPortalRequested = true;
  }
}

bool Wifi::isConfigPortalActive() const {
  return _configPortalActive;
}