#include "stream.h"

size_t readLineFromStream(Stream &stream, char *buffer, const size_t bufferSize) {
  size_t len = stream.readBytesUntil('\n', buffer, bufferSize - 1);
  buffer[len] = '\0';
  return len;
}
