#pragma once

class Ringer
{
public:
    Ringer();

    void init();
    void process();

    void startRinging();
    void stopRinging();

private:
    void setRingerEnabled(bool enabled);

    bool _ringing = false;
    bool _ringState = false;

    unsigned long _ringStartTime = 0;
    unsigned long _lastCycleTime = 0;
};
