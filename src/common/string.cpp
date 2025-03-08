
#include "string.h"

void strTrim(char *str) {
  int start = 0;

  while (str[start] && isspace((unsigned char)str[start])) {
    start++;
  }

  if (start > 0) {
    int i = 0;

    while (str[start + i]) {
      str[i] = str[start + i];
      i++;
    }

    str[i] = '\0';
  }

  int end = strlen(str) - 1;

  while (end >= 0 && isspace((unsigned char)str[end])) {
    str[end] = '\0';
    end--;
  }
}