#pragma once

#include <Arduino.h>

enum class LogLevel
{
    Debug = 0,
    Info,
    Warn,
    Error
};

namespace Logger
{
    extern LogLevel currentLogLevel;

    inline void setLogLevel(LogLevel level)
    {
        currentLogLevel = level;
    }

    inline void debug(const char *message)
    {
        if (currentLogLevel <= LogLevel::Debug)
        {
            Serial.print("[DEBUG] ");
            Serial.println(message);
        }
    }

    inline void debug(const String &message)
    {
        debug(message.c_str());
    }

    inline void info(const char *message)
    {
        if (currentLogLevel <= LogLevel::Info)
        {
            Serial.print("[INFO] ");
            Serial.println(message);
        }
    }

    inline void info(const String &message)
    {
        info(message.c_str());
    }

    inline void warn(const char *message)
    {
        if (currentLogLevel <= LogLevel::Warn)
        {
            Serial.print("[WARN] ");
            Serial.println(message);
        }
    }

    inline void warn(const String &message)
    {
        warn(message.c_str());
    }

    inline void error(const char *message)
    {
        if (currentLogLevel <= LogLevel::Error)
        {
            Serial.print("[ERROR] ");
            Serial.println(message);
        }
    }

    inline void error(const String &message)
    {
        error(message.c_str());
    }
}
