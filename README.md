# TsuryPhone
ESP32 Rotary Phone Firmware

## Overview
This firmware powers a rotary phone conversion based on ESP32. It now supports a generic multi-integration architecture (initial implementation: Home Assistant; mock integration for testing; Android placeholder reserved).

## Multi-Integration Architecture
Core components:
* `IIntegration` – contract for integrations (lifecycle + event callbacks + actions).
* `IntegrationManager` – orchestrates events, state transitions, stats, action dispatch.
* Action Handlers – map dialed number sequences to stable action IDs (`triggerAction`).
* Capability Bitmask – integrations advertise features (`IC_ACTIONS`, `IC_NUMBER_CODE_HANDLER`).

Documentation:
* Preparation Plan: `ANDROID_INTEGRATION_PLAN.md`
* Event Contract: `INTEGRATION_EVENTS.md`
* Developer Guide: `INTEGRATION_DEV_GUIDE.md`
* Test Harness: `INTEGRATION_TEST_HARNESS.md`

## Build Environments (PlatformIO)
Representative environments (see `platformio.ini`):
* `debugWebSerialHA` – Home Assistant integration + WebSerial debugging.
* `debugWebSerialHAMock` – Adds MockIntegration for harness experimentation.
* (Reserved) Android environments with `-DANDROID_INTEGRATION` to be implemented post-preparation.

## Adding a New Integration
1. Create a class deriving from `IIntegration`.
2. Implement required pure virtual methods.
3. Advertise capabilities via `getCapabilities()`.
4. (If using dial codes) Provide `IIntegrationActionHandler` implementation(s) and register in `registerActionHandlers`.
5. Add compile-time registration in `IntegrationManager::registerIntegrations()` under a macro guard.
6. (Optional) Override `getVersion()`.

See the Developer Guide for full details.

## License
See `LICENSE`.

