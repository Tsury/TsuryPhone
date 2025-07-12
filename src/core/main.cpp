#include "main.h"
#include "config/corePhoneConfig.h"
#include "generated/phoneBook.h"
#include "phoneValidation.h"
#include "servers/serverFactory.h"
#include "utils/logger.h"
#include "utils/string.h"

namespace {
  const constexpr int kSerialBaudRate = kModemBaudRate;
  const constexpr int kCheckHardwareTimeout = 1500;
  const constexpr int kCheckLineTimeout = 1500;
  const constexpr int kCallDroppedToneDuration = 3000;
  const constexpr int kDialToneDuration = 1000000;
  const constexpr int kResetToneDuration = 500;
  const constexpr int kWifiPortalToneDuration = 500;
  const constexpr int kToggleVolumeToneDuration = 75;
  const constexpr int kCallWaitingToneDuration = 500;
  const constexpr int kInvalidNumberMp3RepeatCount = 100;
}

PhoneApp::PhoneApp() : _modem(), _ringer(), _hookSwitch(), _rotaryDial(), _wifi() {
  // Initialize state
  _state.newAppState = AppState::Startup;
  _state.prevAppState = AppState::Startup;
  _state.callState = CallState();
  _state.lastModemMessage[0] = '\0';
  _state.messageHandled = false;
  _state.isDnd = false;
  _state.isMaintenanceMode = false;
}

PhoneApp::~PhoneApp() {
  // Clean up global pointer
  if (g_corePhoneConfig == _corePhoneConfig.get()) {
    g_corePhoneConfig = nullptr;
  }
  // Cleanup is automatic with smart pointers
}

void PhoneApp::setupServers() {
  Logger::infoln(F("Setting up servers..."));

  // Let the factory handle all server registration automatically
  ServerFactory::registerAllServers(_serverManager);

  Logger::infoln(F("Server setup complete - %d servers configured"),
                 _serverManager.getServerCount());
}

void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);

  Logger::infoln(F("TsuryPhone starting..."));

  // Initialize core phone config first - this provides shared data across all servers
  _corePhoneConfig = std::make_shared<CorePhoneConfig>();
  g_corePhoneConfig = _corePhoneConfig.get();
  _corePhoneConfig->init();

  _wifi.init();

  // Set up WiFi to control maintenance mode automatically
  _wifi.setMaintenanceModeController([this](bool enabled) { performSetMaintenanceMode(enabled); });

  _modem.init();
  _ringer.init();
  _rotaryDial.init();
  _hookSwitch.init();
  _timeManager.init();

  // Initialize servers
  setupServers();
  _serverManager.setPhoneController(this);
  _serverManager.init();

  Logger::infoln(F("TsuryPhone started!"));

  setState(AppState::CheckHardware);
}

void PhoneApp::loop() {
#ifdef DEBUG
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    SerialAT.write(c);
  }
#endif

  // This is not the best way to do this, but I prefered it to having the state
  // change inside the ringer loop, and have a designated state (e.g. AfterFirstRing)
  // to handle this.
  // Basically I need to know as soon as the first ring is over, so I can play the
  // MP3 for the caller, to fit between the first and second ring.
  // The MP3 is not played immediately, to not surprise the user.
  const bool prevRangAtLeastOnce = _state.callState.rangAtLeastOnce;

  _modem.deriveStateFromMessage(_state);

  _modem.process(_state);
  _wifi.process();
  _hookSwitch.process();
  _rotaryDial.process();
  _ringer.process(_state);
  _timeManager.process(_state);

  // Process all servers
  _serverManager.process(_state);

  const bool afterFirstRing = !prevRangAtLeastOnce && _state.callState.rangAtLeastOnce;

  if (afterFirstRing || _state.prevAppState != _state.newAppState) {
    onStateChanged();
  }

  processState();
}

void PhoneApp::setState(const AppState newState) {
  if (_state.newAppState == newState) {
    return;
  }

  _state.prevAppState = _state.newAppState;
  _state.newAppState = newState;

  onStateChanged();
}

void PhoneApp::onStateChanged() {
  if (_state.newAppState == _state.prevAppState) {
    Logger::infoln(F("Retrying state %s"), appStateToString(_state.newAppState));
  } else {
    Logger::infoln(F("Changing state from %s to %s"),
                   appStateToString(_state.prevAppState),
                   appStateToString(_state.newAppState));
  }

  _stateTime = millis();

  switch (_state.newAppState) {
  case AppState::CheckHardware:
    onStateCheckHardware();
    break;
  case AppState::CheckLine:
    onStateCheckLine();
    break;
  case AppState::Idle:
    onStateIdle();
    break;
  case AppState::IncomingCall:
  case AppState::IncomingCallRing:
    onStateIncomingCall();
    break;
  case AppState::InCall:
    onStateInCall();
    break;
  case AppState::Dialing:
    break;
  default:
    break;
  }
}

void PhoneApp::onStateCheckHardware() {
  _modem.sendCheckHardwareCommand();
}

void PhoneApp::onStateCheckLine() {
  _modem.sendCheckLineCommand();
}

void PhoneApp::onStateIdle() {
  stopEverything();
  _modem.setSpeakerVolume();

  if (_state.callState.otherPartyDropped) {
    _modem.enqueueTone(Tone::CallWaitingTone, kCallDroppedToneDuration);
  }

  if (!_firstTimeSystemReady) {
    _firstTimeSystemReady = true;
    _modem.enqueueMp3(state_ready);

    Logger::infoln(F("System ready!"));
  }
}

void PhoneApp::onStateIncomingCall() {
  CallState &callState = _state.callState;
  char *callNumber = callState.callNumber;

  // Check if number is blocked using core phone config
  if (callNumber[0] != '\0' && _corePhoneConfig->isNumberBlocked(callNumber)) {
    Logger::infoln(F("Blocking call from: %s"), callNumber);
    performHangup();
    setState(AppState::Idle);
    _serverManager.notifyBlockedCall(callNumber);
    return;
  }

  if (callNumber[0] != '\0' && !callState.introducedCaller && callState.rangAtLeastOnce) {
    callState.introducedCaller = true;

    if (hasMp3ForCall(callNumber)) {
      const char *mp3Ptr = getMp3ForCall(callNumber);
      if (mp3Ptr) {
        _modem.enqueueMp3(mp3Ptr);
      }
    }
  } else {
    _ringer.startRinging();
  }
}

void PhoneApp::processStateInvalidNumber() {
  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
    setState(AppState::Idle);
  }
}

void PhoneApp::stopEverything() {
  Logger::infoln(F("Stopping everything..."));
  _modem.stopAllAudio();
  _ringer.stopRinging();
  _rotaryDial.resetCurrentNumber();
}

void PhoneApp::onStateInCall() {
  stopEverything();
  _modem.setEarpieceVolume();
}

void PhoneApp::processState() {
  switch (_state.newAppState) {
  case AppState::CheckHardware:
    processStateCheckHardware();
    break;
  case AppState::CheckLine:
    processStateCheckLine();
    break;
  case AppState::Idle:
    processStateIdle();
    break;
  case AppState::InCall:
    processStateInCall();
    break;
  case AppState::Dialing:
    processStateDialing();
    break;
  case AppState::IncomingCall:
  case AppState::IncomingCallRing:
    processStateIncomingCall();
    break;
  case AppState::InvalidNumber:
    processStateInvalidNumber();
    break;
  default:
    break;
  }
}

void PhoneApp::processStateCheckHardware() {
  if (millis() - _stateTime > kCheckHardwareTimeout) {
    onStateChanged();
  }
}

void PhoneApp::processStateCheckLine() {
  if (millis() - _stateTime > kCheckLineTimeout) {
    onStateChanged();
  }
}

void PhoneApp::processStateIdle() {
  if (_hookSwitch.justChangedOnHook()) {
    stopEverything();
  } else if (_hookSwitch.justChangedOffHook()) {
    _modem.enqueueTone(Tone::DialTone, kDialToneDuration);
  }

  if (_hookSwitch.isOffHook()) {
    DialedNumberResult dialedNumberResult = _rotaryDial.getCurrentNumber();

    if (dialedNumberResult.dialedDigit != kInvalidDialedDigit) {
      char *dialedNumber = dialedNumberResult.callNumber;
      _modem.stopTone();
      _modem.enqueueMp3(dialedDigitsToMp3s[dialedNumberResult.dialedDigit]);

      const DialedNumberValidationResult dialedNumberValidation =
          validateDialedNumber(dialedNumber, _corePhoneConfig.get(), &_serverManager);

      if (dialedNumberValidation == DialedNumberValidationResult::Valid) {
        if (strEqual(dialedNumber, kResetNumber)) {
          _modem.enqueueTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
          ESP.restart();
        } else if (strEqual(dialedNumber, kWifiWebPortalNumber)) {
          _modem.enqueueTone(Tone::GeneralBeep, kWifiPortalToneDuration);
          performSetMaintenanceMode(true);
        } else {
          if (_serverManager.isWebhookEntry(dialedNumber)) {
            if (_serverManager.executeWebhook(dialedNumber)) {
              _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kToggleVolumeToneDuration);
            } else {
              _modem.enqueueTone(Tone::NegativeAcknowledgeOrErrorTone, kResetToneDuration);
            }
            _rotaryDial.resetCurrentNumber();
          } else {
            const char *numberToDial =
                _corePhoneConfig->isPhoneBookEntry(dialedNumber)
                    ? _corePhoneConfig->getPhoneBookNumberForEntry(dialedNumber)
                    : dialedNumber;
            performCall(numberToDial);
            _rotaryDial.resetCurrentNumber();
          }
        }
      } else if (dialedNumberValidation == DialedNumberValidationResult::Invalid) {
        _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
        setState(AppState::InvalidNumber);
      }
    }
  }
}

void PhoneApp::processStateIncomingCall() {
  if (_hookSwitch.justChangedOffHook()) {
    _modem.answer();
  }
}

void PhoneApp::processStateDialing() {
  if (_hookSwitch.justChangedOnHook()) {
    performHangup();
  }
}

void PhoneApp::processStateInCall() {
  if (_hookSwitch.justChangedOnHook()) {
    performHangup();
  }

  const int dialedDigit = _rotaryDial.getDialedDigit();

  if (dialedDigit == 1) {
    Logger::infoln(F("Toggling volume..."));
    _modem.enqueueTone(Tone::PositiveAcknowledgeTone, kToggleVolumeToneDuration);
    _modem.toggleVolume();
  } else if (dialedDigit == 2 && _state.callState.hasCallWaiting()) {
    _modem.switchToCallWaiting();
  }

  if (_state.callState.hasCallWaiting() && !_state.callState.playedCallWaitingTone) {
    _modem.enqueueTone(Tone::IndianDialTone, kCallWaitingToneDuration);
    _state.callState.playedCallWaitingTone = true;
  }

  _rotaryDial.resetCurrentNumber();
}

// IPhoneController interface implementation

void PhoneApp::performCall(const char *number) {
  if (_state.newAppState == AppState::Idle && _hookSwitch.isOnHook()) {
    Logger::infoln(F("External initiated call to: %s"), number);
    _modem.enqueueCall(number);
  }
}

void PhoneApp::performHangup() {
  if (_state.newAppState == AppState::InCall || _state.newAppState == AppState::Dialing) {
    Logger::infoln(F("External initiated hangup"));
    _modem.hangUp();
  }
}

void PhoneApp::performReset() {
  Logger::infoln(F("External initiated reset"));
  ESP.restart();
}

void PhoneApp::performRingWithStructuredPattern(const RingPattern &pattern) {
  Logger::infoln(F("External initiated ring with structured pattern: %d durations, %d repeats"),
                 pattern.durations.size(),
                 pattern.repeats);
  _ringer.startRingingWithStructuredPattern(pattern);
}

void PhoneApp::setDndForceEnabled(bool enabled) {
  _corePhoneConfig->setDndForceEnabled(enabled);
}

void PhoneApp::setDndScheduleEnabled(bool enabled) {
  _corePhoneConfig->setDndScheduleEnabled(enabled);
}

void PhoneApp::setDndHours(int startHour, int startMinute, int endHour, int endMinute) {
  _corePhoneConfig->setDndHours(startHour, startMinute, endHour, endMinute);
}

void PhoneApp::performSetMaintenanceMode(bool enabled) {
  Logger::infoln(F("External set maintenance mode: %s"), enabled ? F("enabled") : F("disabled"));

  // Update both state and shared config
  _state.isMaintenanceMode = enabled;
  _corePhoneConfig->setMaintenanceModeEnabled(enabled);

  if (enabled) {
    // Open WiFi config portal when maintenance mode is enabled
    _wifi.openConfigPortalAsync();
  } else {
    // Close WiFi config portal when maintenance mode is disabled
    _wifi.closeConfigPortal();
  }
}

void PhoneApp::performSwitchToCallWaiting() {
  if (_state.callState.hasCallWaiting()) {
    Logger::infoln(F("External initiated switch to call waiting"));
    _modem.switchToCallWaiting();
  } else {
    Logger::errorln(
        F("External attempted to switch to call waiting but no call waiting available"));
  }
}

void PhoneApp::addQuickDialEntry(const char *name, const char *number) {
  if (_corePhoneConfig) {
    Logger::infoln(F("Adding quick dial entry: %s -> %s"), name, number);
    QuickDialEntry entry;
    entry.name = String(name);
    entry.number = String(number);
    _corePhoneConfig->addQuickDialEntry(entry);
  } else {
    Logger::errorln(F("Cannot add quick dial entry - core config not available"));
  }
}

void PhoneApp::removeQuickDialEntry(const char *name) {
  if (_corePhoneConfig) {
    Logger::infoln(F("Removing quick dial entry: %s"), name);
    _corePhoneConfig->removeQuickDialEntry(String(name));
  } else {
    Logger::errorln(F("Cannot remove quick dial entry - core config not available"));
  }
}