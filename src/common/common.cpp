#include "common.h"

const String appStateToString(AppState state)
{
    switch (state)
    {
    case AppState::Startup:
        return "Startup";
    case AppState::CheckHardware:
        return "CheckHardware";
    case AppState::CheckLine:
        return "CheckLine";
    case AppState::Idle:
        return "Idle";
    case AppState::IncomingCall:
        return "IncomingCall";
    case AppState::IncomingCallRing:
        return "IncomingCallRing";
    case AppState::InCall:
        return "InCall";
    case AppState::Dialing:
        return "Dialing";
    case AppState::WifiTool:
        return "WifiTool";
    default:
        return "Unknown";
    }
}