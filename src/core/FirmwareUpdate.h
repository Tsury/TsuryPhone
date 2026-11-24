#pragma once

#include "../common/state.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <atomic>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>

class WebServer;

// Password-protected firmware updates with automatic rollback.
//
// Every firmware write path (the web API route and the Wi-Fi portal upload page) goes through
// this module. An upload starts only with the correct password (HTTP Basic, any user name) and
// while no call is active and the phone is not ringing. The new image boots pending
// verification; it is marked valid only when Wi-Fi, Home Assistant and SIM network registration
// are healthy within kValidationWindowMs. Otherwise the phone restarts and the bootloader
// returns to the previous slot. A timer armed at boot restarts the phone after
// kResetDeadlineUs even when start-up hangs.
class FirmwareUpdate {
public:
  enum class UploadOutcome : uint8_t {
    None,
    Unauthorized,
    Busy,
    InProgress,
    Writing,
    Failed,
    Installed,
  };

  static FirmwareUpdate &instance();

  // Call first in setup(). Reads the image state and arms the reset timer when pending.
  void beginBoot();
  void process(const State &state);

  void setBusyCheck(std::function<bool()> busyCheck) {
    _busyCheck = busyCheck;
  }
  void setHomeAssistantUrlProvider(std::function<String()> provider) {
    _haUrlProvider = provider;
  }

  bool isAuthorized(const String &authorizationHeader) const;
  bool isBusy() const;

  // Upload session. `owner` identifies the caller (request or portal) so a stale or foreign
  // caller cannot write into another caller's session.
  UploadOutcome beginUpload(const String &authorizationHeader, const void *owner);
  bool writeUpload(const void *owner, const uint8_t *data, size_t len);
  bool finishUpload(const void *owner);
  void abortUpload(const void *owner);
  String lastUploadError() const;

  // Replaces the Wi-Fi portal's own upload page and upload handler with authorized ones.
  // Call from WiFiManager's web server callback, which runs before its routes are added.
  void attachPortal(WebServer &server);

  void populateDiagnostics(JsonObject firmware) const;

private:
  enum class UpdateResult : uint8_t {
    None,
    PendingRestart,
    PendingVerification,
    Success,
    RolledBack,
    UploadFailed,
  };

  FirmwareUpdate();

  void markValid();
  void recordResult(UpdateResult result, const char *pendingSlot);
  void startHomeAssistantProbe();
  static void homeAssistantProbeTask(void *arg);
  static const char *resultName(UpdateResult result);
  void abortLocked();

  void handlePortalUpload(WebServer &server);
  void handlePortalUploadDone(WebServer &server);

  std::function<bool()> _busyCheck;
  std::function<String()> _haUrlProvider;

  SemaphoreHandle_t _sessionMutex;
  const void *_owner = nullptr;
  uint32_t _lastActivityMs = 0UL;
  String _lastError;
  char _targetSlot[17] = {0};
  UploadOutcome _portalOutcome = UploadOutcome::None;

  std::atomic<uint8_t> _result{static_cast<uint8_t>(UpdateResult::None)};
  std::atomic<bool> _restartRequested{false};
  uint32_t _restartRequestedAtMs = 0UL;

  bool _pendingVerification = false;
  bool _networkRegistered = false;
  esp_timer_handle_t _resetTimer = nullptr;

  // Home Assistant probe: -1 not probed, 0 unreachable, 1 answered HTTP.
  std::atomic<int8_t> _haReachable{-1};
  std::atomic<bool> _probeInFlight{false};
  uint32_t _lastProbeMs = 0UL;
};
