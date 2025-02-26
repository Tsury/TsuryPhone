#include "config.h"
#include "ringer.h"
#include "Arduino.h"
#include "common/logger.h"

namespace
{
    constexpr int kRingCycleDuration = 30;
    constexpr int kRingDuration = 2000;
}

void Ringer::init() const
{
    Logger::infoln(F("Initializing ringer..."));

    pinMode(kRingerIn1Pin, OUTPUT);
    pinMode(kRingerIn2Pin, OUTPUT);
    pinMode(kRingerInhPin, OUTPUT);

    setRingerEnabled(false);

    Logger::infoln(F("Ringer initialized!"));
}

void Ringer::startRinging()
{
    if (_ringing)
    {
        return;
    }

    setRingerEnabled(true);
    _ringing = true;
    _ringStartTime = millis();
    _lastCycleTime = millis() + kRingCycleDuration;
    _ringState = false;
}

void Ringer::process()
{
    if (!_ringing)
    {
        return;
    }

    if (millis() - _ringStartTime >= kRingDuration)
    {
        stopRinging();
        return;
    }

    if (millis() - _lastCycleTime >= kRingCycleDuration)
    {
        _ringState = !_ringState;
        _lastCycleTime = millis();

        if (_ringState)
        {
            digitalWrite(kRingerIn1Pin, HIGH);
            digitalWrite(kRingerIn2Pin, LOW);
        }
        else
        {
            digitalWrite(kRingerIn1Pin, LOW);
            digitalWrite(kRingerIn2Pin, HIGH);
        }
    }
}

void Ringer::stopRinging()
{
    if (!_ringing)
    {
        return;
    }

    _ringing = false;
    setRingerEnabled(false);
}

void Ringer::setRingerEnabled(const bool enabled) const
{
    digitalWrite(kRingerInhPin, enabled ? HIGH : LOW);

    if (!enabled)
    {
        digitalWrite(kRingerIn1Pin, LOW);
        digitalWrite(kRingerIn2Pin, LOW);
    }
}