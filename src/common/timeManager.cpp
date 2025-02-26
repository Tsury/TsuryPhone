#include "Arduino.h"
#include "timeManager.h"
#include <time.h>
#include <stdio.h>
#include "common/logger.h"

namespace
{
    constexpr const char *kNtpServer = "pool.ntp.org";
    constexpr long kGmtOffsetSec = 7200;
    constexpr int kDaylightOffsetSec = 3600;
}

void TimeManager::init() const
{
    configTime(kGmtOffsetSec, kDaylightOffsetSec, kNtpServer);
}

bool TimeManager::fetchLocalTime(struct tm &timeinfo) const
{
    if (!getLocalTime(&timeinfo))
    {
        Logger::errorln(F("Failed to obtain time"));
        return false;
    }

    return true;
}

LocalTimeShort TimeManager::getLocalTimeShort() const
{
    LocalTimeShort localTime;
    struct tm timeinfo;
    if (!fetchLocalTime(timeinfo))
    {
        return LocalTimeShort();
    }
    localTime.hour = timeinfo.tm_hour;
    localTime.minute = timeinfo.tm_min;
    return localTime;
}

LocalTimeFull TimeManager::getLocalTimeFull() const
{
    LocalTimeFull localTime = {};
    struct tm timeinfo;

    if (!fetchLocalTime(timeinfo))
    {
        return localTime;
    }

    strftime(localTime.formatted, sizeof(localTime.formatted), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    strftime(localTime.dayOfWeek, sizeof(localTime.dayOfWeek), "%A", &timeinfo);
    strftime(localTime.month, sizeof(localTime.month), "%B", &timeinfo);

    localTime.day = timeinfo.tm_mday;
    localTime.year = timeinfo.tm_year + 1900;
    localTime.hour = timeinfo.tm_hour;
    localTime.minute = timeinfo.tm_min;
    localTime.second = timeinfo.tm_sec;

    return localTime;
}

void TimeManager::process() const
{
    // TODO: Implement
}
