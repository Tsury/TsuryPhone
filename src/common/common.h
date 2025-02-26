#pragma once

#include <Arduino.h>

constexpr size_t kSmallBufferSize = 32;
constexpr size_t kBigBufferSize = 128;

enum class AppState
{
    Startup,
    CheckHardware,
    CheckLine,
    Idle,
    IncomingCall,
    IncomingCallRing,
    InCall,
    Dialing,
    WifiTool
};

const __FlashStringHelper *appStateToString(const AppState state);

size_t readLineFromStream(Stream &stream, char *buffer, const size_t bufferSize);

void trimString(char *str);

inline bool strEqual(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}

inline bool startsWith(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}