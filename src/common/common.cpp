#include "common.h"

const __FlashStringHelper *appStateToString(const AppState state)
{
    switch (state)
    {
    case AppState::Startup:
        return F("Startup");
    case AppState::CheckHardware:
        return F("CheckHardware");
    case AppState::CheckLine:
        return F("CheckLine");
    case AppState::Idle:
        return F("Idle");
    case AppState::IncomingCall:
        return F("IncomingCall");
    case AppState::IncomingCallRing:
        return F("IncomingCallRing");
    case AppState::InCall:
        return F("InCall");
    case AppState::Dialing:
        return F("Dialing");
    case AppState::WifiTool:
        return F("WifiTool");
    default:
        return F("Unknown");
    }
}

size_t readLineFromStream(Stream &stream, char *buffer, const size_t bufferSize)
{
    size_t len = stream.readBytesUntil('\n', buffer, bufferSize - 1);
    buffer[len] = '\0';
    return len;
}

void trimString(char *str)
{
    int start = 0;

    while (str[start] && isspace((unsigned char)str[start]))
    {
        start++;
    }

    if (start > 0)
    {
        int i = 0;

        while (str[start + i])
        {
            str[i] = str[start + i];
            i++;
        }

        str[i] = '\0';
    }

    int end = strlen(str) - 1;

    while (end >= 0 && isspace((unsigned char)str[end]))
    {
        str[end] = '\0';
        end--;
    }
}