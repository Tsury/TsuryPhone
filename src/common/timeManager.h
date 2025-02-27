#pragma once

#include <Arduino.h>

struct LocalTimeShort {
  int hour;
  int minute;
};

struct LocalTimeFull : public LocalTimeShort {
  char formatted[64];
  char dayOfWeek[16];
  char month[16];
  int day;
  int year;
  int second;
};

class TimeManager {
public:
  void init() const;
  void process() const;

  LocalTimeShort getLocalTimeShort() const;
  LocalTimeFull getLocalTimeFull() const;

private:
  bool fetchLocalTime(struct tm &timeinfo) const;
};
