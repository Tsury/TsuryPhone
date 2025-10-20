#include "state.h"

const __FlashStringHelper *appStateToString(const AppState state) {
  switch (state) {
  case AppState::Startup:
    return F("Startup");
  case AppState::CheckHardware:
    return F("CheckHardware");
  case AppState::CheckLine:
    return F("CheckLine");
  case AppState::Idle:
    return F("Idle");
  case AppState::InvalidNumber:
    return F("InvalidNumber");
  case AppState::IncomingCall:
    return F("IncomingCall");
  case AppState::IncomingCallRing:
    return F("IncomingCallRing");
  case AppState::InCall:
    return F("InCall");
  case AppState::Dialing:
    return F("Dialing");
  default:
    return F("Unknown");
  }
}

const __FlashStringHelper *volumeModeToString(const VolumeMode mode) {
  switch (mode) {
  case VolumeMode::Earpiece:
    return F("earpiece");
  case VolumeMode::Speaker:
    return F("speaker");
  default:
    return F("unknown");
  }
}