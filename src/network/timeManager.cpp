#include "timeManager.h"
#include "../config/corePhoneConfig.h"
#include "../utils/logger.h"
#include "config.h"
#include <cstdio>
#include <ctime>

namespace {
  const constexpr char *kNtpServer = "pool.ntp.org";
  const constexpr int kDndCheckIntervalMillis = 60000;
}

void TimeManager::init() const {
  Logger::infoln(F("Initializing time manager..."));

  configTzTime(timeZone, kNtpServer);

  Logger::infoln(F("Time manager initialized!"));
}

bool TimeManager::fetchLocalTime(struct tm &timeinfo) const {
  if (!getLocalTime(&timeinfo)) {
    Logger::errorln(F("Failed to obtain time"));
    return false;
  }

  return true;
}

void TimeManager::process(State &state) {
  uint32_t currentMillis = millis();

  // _lastDndCheckTime != 0 is a workaround for the first time the time manager is called.
  if (currentMillis - _lastDndCheckTime < kDndCheckIntervalMillis && _lastDndCheckTime != 0) {
    return;
  }

  _lastDndCheckTime = currentMillis;

  // Use CorePhoneConfig for unified DnD settings across all servers
  if (!g_corePhoneConfig) {
    Logger::errorln(F("Core phone config not available"));
    state.isDnd = false;
    return;
  }

  // Check if DnD is enabled in core config (either force or schedule mode)
  if (!g_corePhoneConfig->isDndForceEnabled() && !g_corePhoneConfig->isDndScheduleEnabled()) {
    state.isDnd = false;
    Logger::debugln(F("DND disabled in core config"));
    return;
  }

  // If force mode is enabled, DnD is always active
  if (g_corePhoneConfig->isDndForceEnabled()) {
    state.isDnd = true;
    Logger::debugln(F("DND active (force mode)"));
    return;
  }

  struct tm timeinfo;
  if (!fetchLocalTime(timeinfo)) {
    state.isDnd = false;
    Logger::debugln(F("DND off - outside active hours"));
    return;
  }

  // Get DnD hours from core config
  int startHour, startMinute, endHour, endMinute;
  g_corePhoneConfig->getDndHours(startHour, startMinute, endHour, endMinute);

  // Calculate schedule-based DnD
  const int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  const int startMinutes = startHour * 60 + startMinute;
  const int endMinutes = endHour * 60 + endMinute;

  bool isDnd;
  if (startMinutes < endMinutes) {
    isDnd = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
  } else {
    isDnd = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
  }

  state.isDnd = isDnd;

  if (isDnd) {
    Logger::debugln(F("DND active - within scheduled hours"));
  } else {
    Logger::debugln(F("DND off - outside active hours"));
  }
}
