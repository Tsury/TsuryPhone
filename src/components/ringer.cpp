#include "config.h"
#include "ringer.h"
#include "Arduino.h"
#include "common/logger.h"

#define RING_CYCLE_DURATION 30
#define RING_DURATION 2000

Ringer::Ringer()
{
}

void Ringer::init()
{
    Logger::info("Initializing ringer...");

    pinMode(RINGER_IN1_PIN, OUTPUT);
    pinMode(RINGER_IN2_PIN, OUTPUT);
    pinMode(RINGER_INH_PIN, OUTPUT);

    setRingerEnabled(false);

    Logger::info("Ringer initialized!");
}

void Ringer::startRinging()
{
    setRingerEnabled(true);
    _ringing = true;
    _ringStartTime = millis();
    _lastCycleTime = millis();
    _ringState = false;
}

void Ringer::process()
{
    if (!_ringing)
        return;

    if (millis() - _ringStartTime >= RING_DURATION)
    {
        stopRinging();
        return;
    }

    if (millis() - _lastCycleTime >= RING_CYCLE_DURATION)
    {
        _ringState = !_ringState;
        _lastCycleTime = millis();

        if (_ringState)
        {
            digitalWrite(RINGER_IN1_PIN, HIGH);
            digitalWrite(RINGER_IN2_PIN, LOW);
        }
        else
        {
            digitalWrite(RINGER_IN1_PIN, LOW);
            digitalWrite(RINGER_IN2_PIN, HIGH);
        }
    }
}

void Ringer::stopRinging()
{
    _ringing = false;
    setRingerEnabled(false);

    digitalWrite(RINGER_IN1_PIN, LOW);
    digitalWrite(RINGER_IN2_PIN, LOW);
}

void Ringer::setRingerEnabled(bool enabled)
{
    digitalWrite(RINGER_INH_PIN, enabled ? HIGH : LOW);
}