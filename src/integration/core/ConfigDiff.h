#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../../core/DeviceConfig.h"
#include "../IntegrationService.h"
#include <ArduinoJson.h>

// Generic helpers to build config_delta aggregated events safely (no dangling variants)
// Returns true if at least one change was added and writes result JSON to outDoc.

struct ConfigDeltaAggregator {
  IntegrationService &svc;
  JsonDocument doc; // dynamic doc
  JsonArray changesArr;
  bool hasChanges = false;

  ConfigDeltaAggregator(IntegrationService &service) : svc(service) {
    JsonObject root = svc.createEventObject(doc, "config", "config_delta");
    changesArr = root["changes"].to<JsonArray>();
  }

  template <typename TOld, typename TNew>
  void add(const char *key, const TOld &oldValue, const TNew &newValue) {
    JsonObject c = changesArr.add<JsonObject>();
    c["key"] = key;
    c["oldValue"] = oldValue;
    c["newValue"] = newValue;
    hasChanges = true;
  }

  template <typename T> void addIfChanged(const char *key, const T &oldValue, const T &newValue) {
    if (oldValue != newValue) {
      add(key, oldValue, newValue);
    }
  }
};

// DND diff
bool buildDndConfigDelta(IntegrationService &svc,
                         const DndConfig &prev,
                         const DndConfig &curr,
                         JsonDocument &outDoc);
// Audio diff
bool buildAudioConfigDelta(IntegrationService &svc,
                           const AudioConfig &prev,
                           const AudioConfig &curr,
                           JsonDocument &outDoc);

#endif // HOME_ASSISTANT_INTEGRATION