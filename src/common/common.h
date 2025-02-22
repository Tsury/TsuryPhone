#pragma once

#include <Arduino.h>

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

const String appStateToString(AppState state);