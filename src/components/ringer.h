#pragma once

class Ringer
{
public:
    void init() const;
    void process();

    void startRinging();
    void stopRinging();

private:
    void setRingerEnabled(const bool enabled) const;

    bool _ringing = false;
    bool _ringState = false;

    unsigned long _ringStartTime = 0;
    unsigned long _lastCycleTime = 0;
};
