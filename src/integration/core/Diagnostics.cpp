#include "Diagnostics.h"
#include "../../core/DeviceConfig.h"
#include "../../integration/core/DeviceStats.h"
#include "../IntegrationService.h" // For schema version constant

#if defined(HOME_ASSISTANT_INTEGRATION) || defined(ANDROID_INTEGRATION)

void populateCoreDiagnostics(JsonObject root, DeviceConfig &config, DeviceStats &stats) {
  root["schemaVersion"] = INTEGRATION_EVENT_SCHEMA_VERSION;
  root["deviceId"] = config.getDeviceId();
  root["uptimeMs"] = stats.getUptime();
  root["freeHeap"] = stats.getFreeHeap();
  root["rssi"] = stats.getRSSI();

  const CallStats &cs = stats.getCallStats();
  JsonObject metrics = root["metrics"].to<JsonObject>();
  metrics["calls.total"] = cs.totalCalls;
  metrics["calls.in"] = cs.incomingCalls;
  metrics["calls.out"] = cs.outgoingCalls;
  metrics["calls.blocked"] = cs.blockedCalls;
  metrics["calls.talkTimeSeconds"] = cs.totalTalkTimeSeconds;
  metrics["system.heapFree"] = stats.getFreeHeap();
  metrics["system.rssi"] = stats.getRSSI();
}

#endif