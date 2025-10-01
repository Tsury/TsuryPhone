#pragma once

#include <Arduino.h>
#include <functional>

#ifdef HOME_ASSISTANT_INTEGRATION
#include "IntegrationManager.h"
#else
// --- Stub types so core code can be integration-agnostic ---
struct IntegrationCallbackResult {
  bool success;
  String error;
  IntegrationCallbackResult(bool ok = true, const String &err = "") : success(ok), error(err) {}
};

enum class ConfigChangeEvent { NONE, DND_CONFIG_CHANGED, AUDIO_CONFIG_CHANGED };

class IntegrationManager {
public:
  IntegrationManager(class TsuryPhone &, class DeviceConfig &, class State &) {}
  bool init() {
    return true;
  }
  void process() {}
  void setupTsuryPhoneCallbacks(std::function<IntegrationCallbackResult(const String &)>,
                                std::function<IntegrationCallbackResult()>,
                                std::function<IntegrationCallbackResult()>,
                                std::function<IntegrationCallbackResult(const String &)>,
                                std::function<IntegrationCallbackResult()>,
                                std::function<void(const String &)>,
                                std::function<void(bool)> ,
                                std::function<void()> ,
                                std::function<void(ConfigChangeEvent)>) {}
  void enqueueDebugChar(char) {}
  void onFactoryResetInitiated() {}
  void onFactoryResetBeforeRestart() {}
  bool isActionCode(const String &) const {
    return false;
  }
  String resolveActionId(const String &) const {
    return String();
  }
  bool hasPartialActionMatch(const String &) const {
    return false;
  }
  void triggerAction(const String &) {}
};
#endif
