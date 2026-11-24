#include "FirmwareUpdate.h"
#include "../common/logger.h"
#include "../generated/buildInfo.h"
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#ifdef HOME_ASSISTANT_INTEGRATION
#include <HTTPClient.h>
#endif

#ifndef TSURYPHONE_OTA_PASSWORD_SHA256
#error                                                                                             \
    "TSURYPHONE_OTA_PASSWORD_SHA256 is not set; scripts/build_flags.py sets it from the environment"
#endif

// Keep the image pending after boot; this module marks it valid after its health checks.
extern "C" bool verifyRollbackLater() {
  return true;
}

namespace {
  const constexpr uint32_t kValidationWindowMs = 2UL * 60UL * 1000UL;
  const constexpr uint64_t kResetDeadlineUs = 3ULL * 60ULL * 1000ULL * 1000ULL;
  const constexpr uint32_t kRestartDelayMs = 1500UL;
  const constexpr uint32_t kStaleSessionMs = 30000UL;
  const constexpr uint32_t kProbeIntervalMs = 5000UL;
  const constexpr uint16_t kProbeTimeoutMs = 3000;
  const constexpr uint32_t kProbeTaskStack = 6144;
  const constexpr size_t kMaxAuthBytes = 192;
  const constexpr int kHttpOk = 200;
  const constexpr int kHttpBadRequest = 400;
  const constexpr int kHttpConflict = 409;
  const constexpr int kHttpServerError = 500;

  const char *const kPrefsNamespace = "fwupdate";
  const char *const kPrefsSlot = "slot";
  const char *const kPrefsResult = "result";
  const char *const kPortalRealm = "TsuryPhone firmware update";

  const char kPortalUpdatePage[] PROGMEM =
      "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width\">"
      "<title>Firmware update</title></head><body><h3>Firmware update</h3>"
      "<form method=\"POST\" action=\"/u\" enctype=\"multipart/form-data\">"
      "<input type=\"file\" name=\"update\" accept=\".bin\"><br><br>"
      "<input type=\"submit\" value=\"Update\"></form></body></html>";

  class SessionLock {
  public:
    explicit SessionLock(SemaphoreHandle_t mutex) : _mutex(mutex) {
      xSemaphoreTake(_mutex, portMAX_DELAY);
    }
    ~SessionLock() {
      xSemaphoreGive(_mutex);
    }

  private:
    SemaphoreHandle_t _mutex;
  };

  // Compares without an early exit so timing does not reveal how many characters match.
  bool constantTimeEquals(const char *a, const char *b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
      diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
  }

  const char *imageStateName(const esp_partition_t *partition) {
    esp_ota_img_states_t state;
    if (partition == nullptr || esp_ota_get_state_partition(partition, &state) != ESP_OK) {
      return "valid"; // Flashed over USB: no OTA state, the bootloader treats it as valid.
    }
    switch (state) {
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "pending_verification";
    case ESP_OTA_IMG_NEW:
      return "new";
    case ESP_OTA_IMG_INVALID:
    case ESP_OTA_IMG_ABORTED:
      return "invalid";
    default:
      return "valid";
    }
  }

  struct ProbeArgs {
    String url;
  };
} // namespace

FirmwareUpdate &FirmwareUpdate::instance() {
  static FirmwareUpdate update;
  return update;
}

FirmwareUpdate::FirmwareUpdate() : _sessionMutex(xSemaphoreCreateMutex()) {}

void FirmwareUpdate::beginBoot() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  _pendingVerification = running != nullptr &&
                         esp_ota_get_state_partition(running, &state) == ESP_OK &&
                         state == ESP_OTA_IMG_PENDING_VERIFY;

  if (_pendingVerification) {
    // Independent of setup() and loop(): restarts into the previous slot if start-up hangs.
    esp_timer_create_args_t args = {};
    args.callback = [](void *) { esp_restart(); };
    args.name = "fw_validate";
    if (esp_timer_create(&args, &_resetTimer) == ESP_OK) {
      esp_timer_start_once(_resetTimer, kResetDeadlineUs);
    }
  }

  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  const String pendingSlot = prefs.getString(kPrefsSlot, "");
  _result.store(prefs.getUChar(kPrefsResult, static_cast<uint8_t>(UpdateResult::None)));
  prefs.end();

  // An image installed by older firmware leaves no record; its pending state still counts.
  if (running != nullptr) {
    if (!pendingSlot.isEmpty() && pendingSlot != running->label) {
      recordResult(UpdateResult::RolledBack, "");
    } else if (_pendingVerification) {
      recordResult(UpdateResult::PendingVerification, running->label);
    } else if (!pendingSlot.isEmpty()) {
      recordResult(UpdateResult::Success, "");
    }
  }

  Logger::infoln(F("Firmware %s (%s) in %s, image %s, last update %s"),
                 TSURYPHONE_FW_VERSION,
                 TSURYPHONE_FW_COMMIT,
                 running != nullptr ? running->label : "?",
                 imageStateName(running),
                 resultName(static_cast<UpdateResult>(_result.load())));
}

void FirmwareUpdate::process(const State &state) {
  if (_restartRequested.load() && millis() - _restartRequestedAtMs >= kRestartDelayMs &&
      !isBusy()) {
    Logger::infoln(F("Restarting into the new firmware"));
    delay(100);
    ESP.restart();
  }

  if (state.newAppState != AppState::Startup && state.newAppState != AppState::CheckHardware &&
      state.newAppState != AppState::CheckLine) {
    _networkRegistered = true; // The app leaves CheckLine only after SIM network registration.
  }

  if (!_pendingVerification) {
    return;
  }

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
#ifdef HOME_ASSISTANT_INTEGRATION
  if (wifiConnected && _haReachable.load() != 1) {
    startHomeAssistantProbe();
  }
  const bool haReachable = _haReachable.load() == 1;
#else
  const bool haReachable = true; // No Home Assistant link in this build.
#endif

  if (wifiConnected && haReachable && _networkRegistered) {
    markValid();
    return;
  }

  if (millis() >= kValidationWindowMs) {
    Logger::errorln(F("Firmware checks failed (wifi=%d ha=%d network=%d); rolling back"),
                    wifiConnected,
                    haReachable,
                    _networkRegistered);
    delay(100);
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // Only returns when no previous image exists; keep running this one.
    Logger::errorln(F("No previous firmware to roll back to; keeping this image"));
    markValid();
  }
}

void FirmwareUpdate::markValid() {
  esp_ota_mark_app_valid_cancel_rollback();
  if (_resetTimer != nullptr) {
    esp_timer_stop(_resetTimer);
    esp_timer_delete(_resetTimer);
    _resetTimer = nullptr;
  }
  _pendingVerification = false;
  if (static_cast<UpdateResult>(_result.load()) == UpdateResult::PendingVerification) {
    recordResult(UpdateResult::Success, "");
  }
  Logger::infoln(F("Firmware %s marked valid"), TSURYPHONE_FW_VERSION);
}

void FirmwareUpdate::recordResult(UpdateResult result, const char *pendingSlot) {
  _result.store(static_cast<uint8_t>(result));
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putUChar(kPrefsResult, static_cast<uint8_t>(result));
  prefs.putString(kPrefsSlot, pendingSlot);
  prefs.end();
}

void FirmwareUpdate::startHomeAssistantProbe() {
#ifdef HOME_ASSISTANT_INTEGRATION
  const uint32_t now = millis();
  if (_probeInFlight.load() || !_haUrlProvider ||
      (_lastProbeMs != 0UL && now - _lastProbeMs < kProbeIntervalMs)) {
    return;
  }
  _lastProbeMs = now;
  ProbeArgs *args = new ProbeArgs{_haUrlProvider()};
  if (args->url.isEmpty()) {
    _haReachable.store(0);
    delete args;
    return;
  }
  _probeInFlight.store(true);
  if (xTaskCreate(homeAssistantProbeTask, "fw_ha_probe", kProbeTaskStack, args, 1, nullptr) !=
      pdPASS) {
    _probeInFlight.store(false);
    delete args;
  }
#endif
}

void FirmwareUpdate::homeAssistantProbeTask(void *arg) {
#ifdef HOME_ASSISTANT_INTEGRATION
  ProbeArgs *args = static_cast<ProbeArgs *>(arg);
  bool answered = false;
  {
    HTTPClient http;
    http.setConnectTimeout(kProbeTimeoutMs);
    http.setTimeout(kProbeTimeoutMs);
    if (http.begin(args->url)) {
      answered = http.GET() > 0; // Any HTTP status counts as an answer.
      http.end();
    }
  }
  delete args;
  FirmwareUpdate &self = instance();
  self._haReachable.store(answered ? 1 : 0);
  self._probeInFlight.store(false);
#else
  (void)arg;
#endif
  vTaskDelete(nullptr);
}

bool FirmwareUpdate::isAuthorized(const String &authorizationHeader) const {
  static const char kPrefix[] = "Basic ";
  if (!authorizationHeader.startsWith(kPrefix)) {
    return false;
  }
  const String encoded = authorizationHeader.substring(sizeof(kPrefix) - 1);
  unsigned char decoded[kMaxAuthBytes];
  size_t decodedLen = 0;
  if (mbedtls_base64_decode(decoded,
                            sizeof(decoded),
                            &decodedLen,
                            reinterpret_cast<const unsigned char *>(encoded.c_str()),
                            encoded.length()) != 0) {
    return false;
  }
  const unsigned char *colon = static_cast<const unsigned char *>(memchr(decoded, ':', decodedLen));
  if (colon == nullptr) {
    memset(decoded, 0, sizeof(decoded));
    return false;
  }
  const size_t passwordOffset = static_cast<size_t>(colon - decoded) + 1;

  unsigned char digest[32];
  mbedtls_sha256_ret(decoded + passwordOffset, decodedLen - passwordOffset, digest, 0);
  memset(decoded, 0, sizeof(decoded));

  char hex[65];
  for (size_t i = 0; i < sizeof(digest); ++i) {
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  }
  return constantTimeEquals(hex, TSURYPHONE_OTA_PASSWORD_SHA256, 64);
}

bool FirmwareUpdate::isBusy() const {
  return _busyCheck && _busyCheck();
}

FirmwareUpdate::UploadOutcome FirmwareUpdate::beginUpload(const String &authorizationHeader,
                                                          const void *owner) {
  if (!isAuthorized(authorizationHeader)) {
    Logger::warnln(F("Firmware upload rejected: wrong or missing password"));
    return UploadOutcome::Unauthorized;
  }
  if (isBusy()) {
    Logger::warnln(F("Firmware upload refused: call active or phone ringing"));
    return UploadOutcome::Busy;
  }

  SessionLock lock(_sessionMutex);
  if (_owner != nullptr && millis() - _lastActivityMs < kStaleSessionMs) {
    return UploadOutcome::InProgress;
  }
  abortLocked();

  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  if (target == nullptr || !Update.begin(UPDATE_SIZE_UNKNOWN)) {
    _lastError = target == nullptr ? String("no inactive app slot") : String(Update.errorString());
    recordResult(UpdateResult::UploadFailed, "");
    return UploadOutcome::Failed;
  }
  _owner = owner;
  _lastActivityMs = millis();
  _lastError = "";
  strlcpy(_targetSlot, target->label, sizeof(_targetSlot));
  Logger::infoln(F("Firmware upload started into %s"), target->label);
  return UploadOutcome::Writing;
}

bool FirmwareUpdate::writeUpload(const void *owner, const uint8_t *data, size_t len) {
  SessionLock lock(_sessionMutex);
  if (owner == nullptr || owner != _owner) {
    return false;
  }
  _lastActivityMs = millis();
  if (Update.write(const_cast<uint8_t *>(data), len) != len) {
    _lastError = Update.errorString();
    abortLocked();
    recordResult(UpdateResult::UploadFailed, "");
    return false;
  }
  return true;
}

bool FirmwareUpdate::finishUpload(const void *owner) {
  SessionLock lock(_sessionMutex);
  if (owner == nullptr || owner != _owner) {
    return false;
  }
  _owner = nullptr;
  if (!Update.end(true)) {
    _lastError = Update.errorString();
    Update.abort();
    recordResult(UpdateResult::UploadFailed, "");
    Logger::errorln(F("Firmware upload failed: %s"), _lastError.c_str());
    return false;
  }
  recordResult(UpdateResult::PendingRestart, _targetSlot);
  Logger::infoln(F("Firmware written to %s; restarting when idle"), _targetSlot);
  _restartRequestedAtMs = millis();
  _restartRequested.store(true);
  return true;
}

void FirmwareUpdate::abortUpload(const void *owner) {
  SessionLock lock(_sessionMutex);
  if (owner != nullptr && owner == _owner) {
    Logger::warnln(F("Firmware upload aborted"));
    abortLocked();
  }
}

void FirmwareUpdate::abortLocked() {
  if (_owner != nullptr) {
    Update.abort();
    _owner = nullptr;
  }
}

String FirmwareUpdate::lastUploadError() const {
  SessionLock lock(_sessionMutex);
  return _lastError;
}

void FirmwareUpdate::attachPortal(WebServer &server) {
  server.collectHeaders(nullptr, 0); // Collects the Authorization header.
  WebServer *srv = &server;
  server.on("/update", HTTP_ANY, [this, srv]() {
    if (!isAuthorized(srv->header("Authorization"))) {
      srv->requestAuthentication(BASIC_AUTH, kPortalRealm);
      return;
    }
    srv->send_P(kHttpOk, "text/html", kPortalUpdatePage);
  });
  server.on(
      "/u",
      HTTP_POST,
      [this, srv]() { handlePortalUploadDone(*srv); },
      [this, srv]() { handlePortalUpload(*srv); });
}

void FirmwareUpdate::handlePortalUpload(WebServer &server) {
  HTTPUpload &upload = server.upload();
  switch (upload.status) {
  case UPLOAD_FILE_START:
    _portalOutcome = beginUpload(server.header("Authorization"), &server);
    break;
  case UPLOAD_FILE_WRITE:
    if (_portalOutcome == UploadOutcome::Writing &&
        !writeUpload(&server, upload.buf, upload.currentSize)) {
      _portalOutcome = UploadOutcome::Failed;
    }
    break;
  case UPLOAD_FILE_END:
    if (_portalOutcome == UploadOutcome::Writing) {
      _portalOutcome = finishUpload(&server) ? UploadOutcome::Installed : UploadOutcome::Failed;
    }
    break;
  case UPLOAD_FILE_ABORTED:
    abortUpload(&server);
    if (_portalOutcome == UploadOutcome::Writing) {
      _portalOutcome = UploadOutcome::Failed;
    }
    break;
  }
}

void FirmwareUpdate::handlePortalUploadDone(WebServer &server) {
  const UploadOutcome outcome = _portalOutcome;
  _portalOutcome = UploadOutcome::None;
  switch (outcome) {
  case UploadOutcome::Installed:
    server.send(kHttpOk, "text/plain", "Firmware installed; the phone restarts now.");
    break;
  case UploadOutcome::Busy:
    server.send(kHttpConflict, "text/plain", "Refused: a call is active or the phone is ringing.");
    break;
  case UploadOutcome::InProgress:
    server.send(kHttpConflict, "text/plain", "Refused: another upload is in progress.");
    break;
  case UploadOutcome::Failed:
  case UploadOutcome::Writing:
    server.send(kHttpServerError, "text/plain", "Update failed: " + lastUploadError());
    break;
  case UploadOutcome::None:
    if (isAuthorized(server.header("Authorization"))) {
      server.send(kHttpBadRequest, "text/plain", "No firmware file in the request.");
      break;
    }
    server.requestAuthentication(BASIC_AUTH, kPortalRealm);
    break;
  case UploadOutcome::Unauthorized:
  default:
    server.requestAuthentication(BASIC_AUTH, kPortalRealm);
    break;
  }
}

void FirmwareUpdate::populateDiagnostics(JsonObject firmware) const {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const UpdateResult result = static_cast<UpdateResult>(_result.load());
  firmware["version"] = TSURYPHONE_FW_VERSION;
  firmware["commit"] = TSURYPHONE_FW_COMMIT;
  firmware["buildTime"] = TSURYPHONE_FW_BUILD_TIME;
  firmware["slot"] = running != nullptr ? running->label : "";
  firmware["slotSize"] = running != nullptr ? running->size : 0;
  firmware["imageState"] = imageStateName(running);
  firmware["lastUpdateResult"] = resultName(result);
  firmware["rolledBack"] = result == UpdateResult::RolledBack;
  firmware["busy"] = isBusy();

  JsonObject health = firmware["health"].to<JsonObject>();
  health["wifi"] = WiFi.status() == WL_CONNECTED;
  health["networkRegistered"] = _networkRegistered;
  const int8_t ha = _haReachable.load();
  if (ha < 0) {
    health["homeAssistant"] = nullptr;
  } else {
    health["homeAssistant"] = ha == 1;
  }
}

const char *FirmwareUpdate::resultName(UpdateResult result) {
  switch (result) {
  case UpdateResult::PendingRestart:
    return "pending_restart";
  case UpdateResult::PendingVerification:
    return "pending_verification";
  case UpdateResult::Success:
    return "success";
  case UpdateResult::RolledBack:
    return "rolled_back";
  case UpdateResult::UploadFailed:
    return "upload_failed";
  case UpdateResult::None:
  default:
    return "none";
  }
}
