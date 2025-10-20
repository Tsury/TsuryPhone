#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <ArduinoJson.h>

class DeviceConfig;
class DeviceStats;

// Populates a diagnostics JSON root object with generic (integration-neutral) data.
// Fields added:
//  schemaVersion, deviceId, uptimeMs, freeHeap, rssi, metrics (calls.*, system.*)
// Does NOT add integration-specific capability info.
void populateCoreDiagnostics(JsonObject root, DeviceConfig &config, DeviceStats &stats);

#endif // HOME_ASSISTANT_INTEGRATION