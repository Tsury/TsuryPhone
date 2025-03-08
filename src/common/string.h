#pragma once

#include <Arduino.h>

void strTrim(char *str);

inline bool strEqual(const char *str1, const char *str2) {
  return strcmp(str1, str2) == 0;
}

inline bool strStartsWith(const char *str, const char *prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}