#pragma once

#include <Arduino.h>

// Reads one line (without '\n') into buffer. A line longer than the buffer is drained to its
// end and reported through truncated, so its tail never reads as a separate line.
size_t readLineFromStream(Stream &stream,
                          char *buffer,
                          const size_t bufferSize,
                          bool *truncated = nullptr);
