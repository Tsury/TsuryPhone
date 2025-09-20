# Integration Developer Guide

Status: Stable (Preparation Phase)
Last Updated: 2025-09-06

This guide explains how to add a new integration (e.g., Android) to the firmware using the generic multi-integration infrastructure.

---
## 1. Concepts Overview
* IIntegration: Core lifecycle + event sink interface.
* IntegrationManager: Owns all integrations, broadcasts events, enumerates action handlers.
* Action Handlers: Implement `IIntegrationActionHandler` to translate dialed codes to stable action ids.
* Capabilities: Bitmask returned by `getCapabilities()` advertising support (e.g., IC_ACTIONS).
* Events: See `INTEGRATION_EVENTS.md` for contract & ordering.

---
## 2. Minimal Integration Checklist
1. Create `YourIntegration.h/.cpp` under `src/integration/your/`.
2. Derive from `IIntegration` and implement required pure virtual methods:
   * init / process / stop
   * update* event functions
   * report* statistics + triggerAction
   * onConfigurationChanged
   * getName / getTag / isEnabled / getCapabilities
3. (Optional) Override `getVersion()` if you want a semantic version string for logs.
4. (If actions) Implement one or more `IIntegrationActionHandler` classes and register them in `registerActionHandlers(IntegrationManager&)`.
5. Add compile flag macro (e.g., `-DANDROID_INTEGRATION`) and registration block in `IntegrationManager::registerIntegrations()`.
6. Build with the new environment and confirm startup log lists capabilities.

---
## 3. Required Interface Methods
Implementations must be fast and non-blocking. Heavy IO should be deferred or internally rate-limited.

| Method | Purpose | Notes |
|--------|---------|-------|
| init() | One-time setup | Return false to disable integration gracefully |
| process() | Called each loop tick | Keep under a few ms |
| stop() | Cleanup | Optional if no resources |
| updatePhoneState | High-level app state changes | Called only on transitions |
| updateCallInfo | Number + direction | Called when number changes / known |
| updateDialingProgress | Partial user input | Use for predictive feedback |
| updateRingState | Incoming ring boolean | Derived from state machine |
| updateSystemStatus | Periodic health | Manager controls cadence |
| updateDndState | DND edge | Only on change |
| triggerAction | Generic action execution | Filter by prefix if not all actions apply |
| reportCallStart/End/Blocked | Lifecycle statistics | Optional internal logging |
| reportError | Error surfacing | Use structured logs |
| onConfigurationChanged | Config mutated | Query DeviceConfig as needed |
| getName/getTag | Identity | Tag <= 8 chars for concise logs |
| isEnabled | Runtime gating | Return false to skip processing |
| getCapabilities | Advertise features | Combine IntegrationCapability bits |
| registerActionHandlers | Register any dial code handlers | Only if IC_NUMBER_CODE_HANDLER / IC_ACTIONS relevant |
| getVersion (optional) | Version string | Return string literal w/ static storage |

---
## 4. Capabilities Bitmask
```
IC_ACTIONS              // integration reacts to generic action ids
IC_NUMBER_CODE_HANDLER  // integration contributes dial code parsing
```
Add new bits carefully; ensure they are logged in `IntegrationManager::registerIntegrations()`.

---
## 5. Action Handlers
Interface (excerpt):
```
class IIntegrationActionHandler {
 public:
  virtual bool isPartialMatch(const String&) const = 0;
  virtual bool isFullMatch(const String&) const = 0;
  virtual String resolveActionId(const String&) const = 0;
  virtual std::vector<String> listFullCodes() const { return {}; }
};
```
Guidelines:
* Keep matching O(length_of_input) – avoid heavy allocations.
* Return stable action id strings (namespaced, e.g. "ha.webhook.front_door").
* Use enumeration (`listFullCodes`) for conflict detection & debug dumps.

---
## 6. Event Ordering & Idempotency
See `INTEGRATION_EVENTS.md`. Handlers should be idempotent; guard against duplicate transitions when re-sent within the same loop.

### Process Loop Ordering (M1)
Within `IntegrationManager::process()` the sequence is:
1. `StatsManager.process()` updates rolling stats.
2. `checkForStateChanges()` emits state transition callbacks in this internal order:
   * Phone state transition detection (+ call start/end side-effects)
   * Call number change → `updateCallInfo`
   * Dialing progress change → `updateDialingProgress`
   * DND change → `updateDndState`
   * Maintenance mode change (triggers `updatePhoneState` for consistency)
   * Ringing state change → `updateRingState`
3. Each enabled integration `process()` is invoked.

Integrations should not assume they run before state emission—state callbacks always precede integration `process()` in the same loop tick.

---
## 7. Configuration Changes
When device config mutates, manager calls `onConfigurationChanged()`. Pull specific values from `DeviceConfig` rather than caching global singletons.

---
## 8. Logging
Use `INT_LOG_INFO(getTag(), ...)` etc. Debug logs behind runtime toggle: `enableIntegrationDebugLogging(true)`.

---
## 9. Version Reporting (Optional)
Override `getVersion()` returning a string literal. Manager may later include versions in its startup summary.

---
## 10. Testing with Mock Integration
Enable `-DMOCK_INTEGRATION` environment (`env:debugWebSerialHAMock`), then use serial to provoke events. Run `listActionCodes()` (if exposed) to verify handler registration.

---
## 11. Adding Android (Future Outline)
1. Add build env with `ANDROID_INTEGRATION` flag (already reserved).
2. Implement `AndroidIntegration` using Android transport (post-prep phase).
3. Provide action handler if Android needs dial codes beyond HA.
4. Validate sequence with mock + HA concurrently enabled.

---
## 12. Common Pitfalls
* Long blocking network calls in `process()` – use internal state machine and incremental progress.
* Emitting actions directly from handler – only handlers map codes; manager invokes `triggerAction`.
* Forgetting to set capability bits – results in silent non-participation.

---
## 13. Future Enhancements
* Add JSON logging capability flag.
* Introduce metrics export hook (see plan N2).

---
---
## 14. Verification Checklist (D1 Finalization)
Before submitting a new integration PR:
[] Builds succeed for all enabled envs (baseline + new macro).
[] Startup log lists integration with expected capabilities.
[] `triggerAction` path exercised (if IC_ACTIONS set).
[] No blocking calls > ~10ms inside `process()` (instrument with timestamps if unsure).
[] Configuration changes reflected after `onConfigurationChanged()` (spot-check one setting).
[] No duplicate action codes (check log for conflict warnings).
[] Optional: Version string prints (if `getVersion()` overridden).

End of Guide.

---
## Appendix A: High-Level Architecture Diagram (D3)
```mermaid
flowchart LR
   subgraph PhoneApp
      A[Main Loop]
      A --> B[IntegrationManager]
   end
   B -->|State & Events| I1[HAIntegration]
   B -->|State & Events| I2[MockIntegration]
   B -->|Future| I3[AndroidIntegration]
   subgraph ActionHandlers
      H1[HANumberHandler]
      %% Additional handlers can be added here
   end
   H1 --> B
   A -->|Dial Input| H1
   B -->|triggerAction(actionId)| I1
   B -->|triggerAction(actionId)| I2
   B -->|triggerAction(actionId)| I3
   B -->|Stats / Config| S[(DeviceStats / DeviceConfig)]
```

