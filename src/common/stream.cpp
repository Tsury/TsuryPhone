#include "stream.h"

size_t readLineFromStream(Stream &stream, char *buffer, const size_t bufferSize, bool *truncated) {
  size_t len = stream.readBytesUntil('\n', buffer, bufferSize - 1);
  buffer[len] = '\0';

  bool overflow = false;
  if (len == bufferSize - 1) {
    char rest[32];
    size_t restLen = 0;
    do {
      restLen = stream.readBytesUntil('\n', rest, sizeof(rest));
      // A lone '\r' is the end of a line that fit exactly.
      overflow = overflow || restLen > 1 || (restLen == 1 && rest[0] != '\r');
    } while (restLen == sizeof(rest));
  }

  if (truncated != nullptr) {
    *truncated = overflow;
  }
  return len;
}
