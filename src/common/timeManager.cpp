#include "timeManager.h"
#include "config.h"
#include "logger.h"
#ifdef HOME_ASSISTANT_INTEGRATION
#include "homeAssistantServer.h"
#endif
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

  struct tm timeinfo;

  if (!fetchLocalTime(timeinfo)) {
    state.isDnd = false;
    return;
  }

  int startHour, startMinute, endHour, endMinute;

#ifdef HOME_ASSISTANT_INTEGRATION
  // When HA integration is enabled, ONLY use HA DnD settings

  // Check if HA has force DnD enabled (takes priority over everything)
  if (isDndForceEnabled()) {
    state.isDnd = true;
    Logger::debugln(F("DND force enabled by HA"));
    return;
  }

  // Check if schedule-based DnD is enabled
  if (!isDndScheduleEnabled()) {
    state.isDnd = false;
    Logger::debugln(F("HA DND schedule disabled"));
    return;
  }

  // Get HA DnD hours for schedule
  getHaDndHours(startHour, startMinute, endHour, endMinute);

  // If HA has custom hours set in state, use those instead
  // Note: -1 means "not set", 0 is valid (midnight)
  if (state.haDndStartHour != -1) {
    startHour = state.haDndStartHour;
    startMinute = state.haDndStartMinute;
    endHour = state.haDndEndHour;
    endMinute = state.haDndEndMinute;
  }
#else

  if (!kDndEnabled) {
    state.isDnd = false;
    return;
  }

  // When HA integration is disabled, use local config settings
  startHour = kDndStartHour;
  startMinute = kDndStartMinute;
  endHour = kDndEndHour;
  endMinute = kDndEndMinute;
#endif

  // Now we calculate the schedule-based DnD
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
}
