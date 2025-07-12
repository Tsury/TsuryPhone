#pragma once

#include <Arduino.h>

size_t readLineFromStream(Stream &stream, char *buffer, const size_t bufferSize);
