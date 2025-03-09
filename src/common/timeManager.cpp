#include "timeManager.h"
#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <time.h>

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
  unsigned long currentMillis = millis();

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

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = kDndStartHour * 60 + kDndStartMinute;
  int endMinutes = kDndEndHour * 60 + kDndEndMinute;

  bool isDnd;

  if (startMinutes < endMinutes) {
    isDnd = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
  } else {
    isDnd = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
  }

  state.isDnd = isDnd;

  Logger::debugln(F("Current time: %02d:%02d"), timeinfo.tm_hour, timeinfo.tm_min);
  Logger::debugln(F("DND state: %s"), state.isDnd ? F("true") : F("false"));
}
