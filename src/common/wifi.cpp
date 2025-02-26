#include "wifi.h"
#include "common/logger.h"

void Wifi::init()
{
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    // reset settings - wipe credentials for testing
    // wm.resetSettings();

    _wifiManager.setConfigPortalBlocking(false);
    _wifiManager.setConfigPortalTimeout(60);
    _wifiManager.setDebugOutput(true);

    // automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if (_wifiManager.autoConnect("TsuryPhone"))
    {
        Logger::infoln(F("connected...yeey :)"));
    }
    else
    {
        Logger::infoln(F("Configportal running"));
    }
}

void Wifi::process()
{
    _wifiManager.process();
}