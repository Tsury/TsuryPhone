#include "timeManager.h"
#include "../core/DeviceConfig.h"
#include "config.h"
#include "logger.h"
#include <WiFi.h>
#include <cstdio>
#include <ctime>

namespace {
  const constexpr char *kNtpServer = "pool.ntp.org";
  const constexpr int kDndCheckIntervalMillis = 60000;
}

TimeManager::TimeManager(DeviceConfig &config) : _config(config) {}

void TimeManager::init() const {
  Logger::infoln(F("Initializing time manager..."));

  configTzTime(timeZone, kNtpServer);

  Logger::infoln(F("Time manager initialized!"));
}

bool TimeManager::fetchLocalTime(struct tm &timeinfo) const {
  if (WiFi.status() != WL_CONNECTED) {
    Logger::warnln(F("Skipping time sync: WiFi not connected"));
    return false;
  }

  if (!getLocalTime(&timeinfo)) {
    Logger::warnln(F("Failed to obtain time from NTP (will retry)"));
    return false;
  }

  return true;
}

void TimeManager::process(State &state) {
  determineDndState(state);
}

void TimeManager::determineDndState(State &state) {
  uint32_t currentMillis = millis();

  // _lastDndCheckTime != 0 is a workaround for the first time the time manager is called.
  if (currentMillis - _lastDndCheckTime < kDndCheckIntervalMillis && _lastDndCheckTime != 0) {
    return;
  }

  _lastDndCheckTime = currentMillis;

  const DndConfig &dndConfig = _config.getDndConfig();

  // Check force DND first
  if (dndConfig.force) {
    state.isDnd = true;
    return;
  }

  // If scheduled DND is disabled, no DND
  if (!dndConfig.scheduled) {
    state.isDnd = false;
    return;
  }

  struct tm timeinfo;

  if (!fetchLocalTime(timeinfo)) {
    state.isDnd = false;
    return;
  }

  const int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  const int startMinutes = dndConfig.startHour * 60 + dndConfig.startMinute;
  const int endMinutes = dndConfig.endHour * 60 + dndConfig.endMinute;

  bool isDnd;

  if (startMinutes < endMinutes) {
    isDnd = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
  } else {
    isDnd = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
  }

  state.isDnd = isDnd;
}