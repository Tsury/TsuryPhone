# Android Integration Preparation Plan

Status: Completed (Preparation Phase)  
Last Updated: 2025-09-06 (Finalized)  
Owner: (assign)  
Version: 0.1

> Scope of this document: ONLY infra preparation to allow adding an Android integration later. No Android transport / protocol / feature implementation yet.

Build Verification Rule: After completing each task (or logical sub-task) the firmware MUST be compiled using:
`C:\Users\Tsury\.platformio\penv\Scripts\platformio.exe run --environment debugWebSerialHA`
The plan's progress updates should explicitly state the build result (PASS/FAIL) before moving to the next item.

---

## 1. Context & Motivation

The project currently supports a single integration (Home Assistant). The architecture is largely generic (via `IIntegration`, `IntegrationManager`, and `IntegrationService`), yet a few HA‑centric assumptions exist (webhook naming, config change enums, manager helper methods that downcast to `HAIntegration`). Before beginning Android support we want to:

1. Eliminate HA‑specific leakage from generic layers.  
2. Introduce explicit capability + action abstractions (replacing former webhook concepts).  
3. Harden configuration / event broadcasting so multiple integrations can coexist cleanly.  
4. Improve testability & observability (so we can verify multi-integration behaviour with low friction).  
5. Document extension points clearly (developer on‑ramp for future integrations).  

Non-goal: Implement Android communication (BLE / WebSocket / ADB / USB). That will happen AFTER all “Preparation” tasks below reach Done.

---

## 2. High-Level Objectives (Preparation Phase)

1. Provide a clean, integration-agnostic surface in `IntegrationManager` (no downcasting).  
2. Standardize “action triggers” (replacing webhook-only semantics).  
3. Refine configuration change signaling to remove HA-only enum members or isolate them.  
4. Optional but valuable: capability bitmask / feature flags for integrations.  
5. Ensure shared logic duplication (config change callbacks inside both Manager & IntegrationService) is removed or purposeful.  
6. Add build scaffolding (macro + env sections) WITHOUT adding Android source yet (gated until later).  
7. Provide a lightweight test harness plan for state & action flow.  
8. Document extension steps (checklist for future integrators).  

---

## 3. Non-Goals

| Item | Reason |
|------|--------|
| Android transport design | Outside preparation scope |
| UI / frontend updates | Not required for infra readiness |
| Performance micro-optimizations | Defer until multi-integration proves overhead |
| Telemetry / analytics export | Nice-to-have, not blocking |

---

## 4. Historical Baseline (Pre-Preparation)

| Component | Role | Noted Issues / Coupling |
|-----------|------|-------------------------|
| `IIntegration` | Lifecycle + callbacks | No capability discovery; webhook concept absent here (good) |
| `IntegrationManager` | Orchestrates broadcast & state change detection | HA-specific helper functions (`isWebhookTrigger`, etc.) + direct `HAIntegration` downcast |
| `IntegrationService` | Business logic + JSON serialization | Single config change callback (unlike manager’s vector) |
| Config Change Enum | Contains HA-specific members (`WEBHOOK_ACTION_CHANGED`, `HA_URL_CHANGED`) | Leaks HA specifics globally |
| Action Triggering | `triggerWebhook()` naming at manager + per integration | Must be renamed to generic `triggerAction()` |
| Number Handling | Webhook detection logic accessible through `IntegrationManager` HA helpers | Not generalizable yet |
| Build System (`platformio.ini`) | HA macro only (`HOME_ASSISTANT_INTEGRATION`) | No reserved Android env/macro |

---

## 5. Gap Analysis & Improvement Themes (Historical)

1. HA Leakage: Remove HA-specific helper methods from manager or re-express generically.  
2. Action Abstraction: Introduce generic “Integration Action” (code -> action id -> invoke) fully replacing webhook terminology outside HA-specific code.  
3. Enum Hygiene: Split config change events into: core device config vs integration-specific extension events. Provide a neutral extensibility path.  
4. Callback Consolidation: Choose a single layer to multiplex config change callbacks (prefer manager).  
5. Capability Discovery: Add feature flags (e.g., SupportsActions, SupportsPushEvents, SupportsNumberCodeParsing).  
6. Number Code Strategy Registry: Integration(s) register objects that parse partial & complete dial sequences.  
7. Testability: Add mock integration + unit test strategy (even if tests not yet implemented on embedded target, provide harness plan).  
8. Observability: Standard log prefixes and structured log shape for multi-integration debugging.  
9. Documentation: Developer extension guide with steps & interface contract.  

---

## 6. Task Breakdown & Tracking

Legend: Effort (S = <30m, M = 30–90m, L = >90m).  
Status Keys: [ ] TODO, [P] In Progress, [R] Review, [X] Blocked, [D] Done.

### 6.1 Core Abstractions
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| A1 | Introduce `IntegrationCapability` enum + `uint32_t getCapabilities()` in `IIntegration` | M | [D] | Implemented (IC_ACTIONS, IC_NUMBER_CODE_HANDLER) |
| A2 | Replace manager HA helpers (`isWebhookTrigger`, etc.) with generic action query API | M | [D] | Registry + handler interface + auto-registration implemented |
| A3 | Replace `triggerWebhook` with `triggerAction(const String &id)` (remove old method entirely) | S | [D] | Implemented earlier (interface + usage) Build PASS |
| A4 | Extract number code parsing to pluggable interface `INumberCodeHandler` | M | [D] | Implemented as `IIntegrationActionHandler` (already used by HANumberHandler) |
| A5 | Add registry management in `IntegrationManager` (`addNumberCodeHandler`, `queryPartialMatch`, etc.) | M | [D] | Implemented: `addActionHandler`, partial/full/resolve APIs |

### 6.2 Config & Events
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| C1 | Refactor `ConfigChangeEvent` enum: move HA-specific items behind `#ifdef` or rename generically | M | [D] | Implemented: removed HA-specific, added INTEGRATION_EXTENSION_CHANGED (Build PASS) |
| C2 | Remove duplicate config callback inside `IntegrationService` or convert to vector for internal use only | S | [D] | Removed internal callback (Build PASS) |
| C3 | Document event emission contract (when each update* method should be called) | S | [D] | External doc `INTEGRATION_EVENTS.md` + in-plan notes |
| C4 | Add structured log context macro `INT_LOG(name, msg, ...)` | S | [D] | Macros integrated across codebase + runtime debug toggle flag |

### 6.3 Build & Conditional Compilation
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| B1 | Reserve macro `ANDROID_INTEGRATION` in `platformio.ini` (no sources yet) | S | [D] | Added base_android + debug/release envs Build PASS |
| B2 | Add static assert / compile-time warning if no integrations enabled | S | [D] | Added warning in main.cpp + #error in IntegrationManager.h (Build PASS) |

### 6.4 Testing & Validation Prep
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| T1 | Add `MockIntegration` (under `test/` or `src/integration/mock/`) for unit harness | M | [D] | Implemented `MockIntegration` capturing events (behind MOCK_INTEGRATION) |
| T2 | Sketch test harness doc (state transitions → expected calls) | S | [D] | `INTEGRATION_TEST_HARNESS.md` created |
| T3 | Add debug command / serial command to list capabilities & registered handlers | S | [D] | Added `IntegrationManager::listActionCodes()` |

### 6.5 Documentation
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| D1 | Write Integration Extension Guide (`INTEGRATION_DEV_GUIDE.md`) | M | [D] | Guide stabilized with verification checklist |
| D2 | Update existing README with multi-integration statement | S | [D] | README updated with architecture + docs refs |
| D3 | Add UML-ish component diagram (optional) | M | [D] | Mermaid diagram in dev guide |

### 6.6 Cleanup / Misc
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| M1 | Ensure `IntegrationManager::process()` order documented (stats → state changes → integrations) | S | [D] | Documented in dev guide |
| M2 | Add `const char* getVersion()` optional to `IIntegration` (default nullptr) | S | [D] | Interface added (implementations may override) |
| M3 | Add guard: if two handlers claim same action code, log conflict | S | [D] | Implemented enumeration + conflict warnings |
| M4 | Runtime debug logging toggle (global flag & API) | S | [D] | Implemented `setIntegrationDebugLogging(bool)` |

### 6.7 Nice-To-Haves (Implemented)
| ID | Task | Effort | Status | Notes |
|----|------|--------|--------|-------|
| N1 | Shared singleton `IntegrationService` (if memory pressure emerges) | L | [D] | Implemented singleton via `IntegrationService::shared()` |
| N2 | Metrics snapshot export command | M | [D] | Serial 'm' command calls `exportMetricsSnapshot()` |
| N3 | Structured JSON logging channel | L | [D] | JSON logs with seq+ts; 'j' (detail) & 'k' (kv) commands |

---

## 7. Detailed Design Notes (Preparation Changes)

### C3: Event Emission Contract
Documenting when each integration callback is invoked by `IntegrationManager` ensures future integrations behave consistently.

Event categories and triggers:
1. Phone State Changes (`updatePhoneState(newState, previousState)`)
	- Fired only when high-level `AppState` changes (detected in manager loop). Not on every process tick.
2. Call Info (`updateCallInfo(number, isIncoming, startTime)`)
	- Emitted when entering dialing/outgoing/incoming states and number context becomes stable or changes significance.
3. Dialing Progress (`updateDialingProgress(currentNumber)`)
	- Emitted on each user digit input when the sequence is still potentially an action or a phone number (partial matches exist) or when displayed number changes.
4. System Status (`updateSystemStatus()`)
	- Periodic health / stats emission (interval-based or after notable subsystem transitions). Rate-limited inside manager.
5. DND State (`updateDndState(isDndActive)`)
	- Emitted only when DND toggles (edge-triggered).
6. Call Lifecycle (`reportCallStart(number, isIncoming)` / `reportCallEnd(duration)`)
	- Start fires when call transitions into active conversation state. End fires exactly once per started call with computed duration.
7. Actions (`triggerAction(actionId)`)
	- Initiated by user dialing a code resolved by a registered handler. All integrations supporting `IC_ACTIONS` may interpret the id. Handlers themselves are passive parsers; integrations decide whether to act.

Ordering Guarantees (within a single loop iteration):
`updatePhoneState` (if applicable) precedes specific updates like `updateCallInfo` and `reportCallStart`. Dialing progress may precede a final `triggerAction` once a full match is confirmed.

Future Android integration MUST treat these callbacks as idempotent within a loop iteration (no heavy side effects without change detection).

### A2/A3: Generic Action Trigger (Webhook Replacement)
Introduce two new abstractions:
```cpp
struct IntegrationActionDescriptor {
	String id;          // Stable action identifier (e.g., "webhook.door_open")
	String label;       // Human readable (optional)
	uint16_t features;  // Bit flags (e.g., requiresOnline, destructive)
};

class IIntegrationActionHandler {
public:
	virtual ~IIntegrationActionHandler() = default;
	virtual bool isPartialMatch(const String &dialed) const = 0; // For progressive feedback
	virtual bool isFullMatch(const String &dialed) const = 0;    // When to trigger
	virtual String resolveActionId(const String &dialed) const = 0; // Map dialed code → action id
};
```
`IntegrationManager` maintains: `std::vector<IIntegrationActionHandler*> _actionHandlers;`

Flow: Dial digit → manager queries partial match for UX → on full match & confirmation → `triggerAction(id)` (broadcast to integrations that advertise capability & choose to act). HA’s former webhook logic becomes an action handler implementation. The previous `triggerWebhook()` entry point is removed (no deprecation layer) to avoid perpetuating HA-centric vocabulary.

### A4/A5: Number Code Strategy
We unify number code & action handling (above). If another non-action code type appears later (e.g., service menu), it can be another handler instance.

### C1: Config Change Enum Refactor
Current (example excerpt):
```
enum class ConfigChangeEvent { DND_CONFIG_CHANGED, AUDIO_CONFIG_CHANGED, QUICK_DIAL_CHANGED,
	BLOCKED_NUMBER_CHANGED, WEBHOOK_ACTION_CHANGED, HA_URL_CHANGED, RING_PATTERN_CHANGED };
```
Proposed core-safe subset:
```
enum class ConfigChangeEvent : uint8_t {
	DND_CONFIG_CHANGED,
	AUDIO_CONFIG_CHANGED,
	QUICK_DIAL_CHANGED,
	BLOCKED_NUMBER_CHANGED,
	RING_PATTERN_CHANGED,
	INTEGRATION_EXTENSION_CHANGED  // Generic bucket
};
```
If `HOME_ASSISTANT_INTEGRATION` defined, map old events to new: 
Former enum values `WEBHOOK_ACTION_CHANGED` & `HA_URL_CHANGED` will no longer exist in the generic enum. HA-specific code will emit `INTEGRATION_EXTENSION_CHANGED` and internally differentiate its sub-type (e.g. via a secondary enum or querying updated HA config on notification). All generic code references only the neutral enum values.

Backward compatibility path: Provide transitional `#ifdef` block with legacy enum values aliasing new ones (optional if footprint allows).

### C2: Config Callback Consolidation
Remove `_configChangeCallback` from `IntegrationService`; all config mutations call a helper that forwards to `IntegrationManager::notifyConfigChange`. Integrations needing intermediate transforms can still subscribe through manager.

### A1: Capabilities Bitmask
```
`enum IntegrationCapability : uint32_t {
	IC_NONE = 0,
	IC_ACTIONS = 1 << 0,
	IC_NUMBER_CODE_HANDLER = 1 << 1
};
virtual uint32_t getCapabilities() const { return IC_NONE; }
```
Integrations override if they expose those features. Manager logs a capability summary at startup. No legacy webhook capability flag remains; actions are the unified mechanism.

### Logging Improvements (C4)
Macro example:
```
#define INTLOG(name, level, fmt, ...) Logger::level(F("[%s] " fmt), name, ##__VA_ARGS__)
```
Ensures uniform prefixing: `[HomeAssistant] Initializing ...`.

Refined design (to implement under C4):
```
enum LogLevel { LL_DEBUG, LL_INFO, LL_WARN, LL_ERROR };

#ifdef ENABLE_INT_LOG_DEBUG
	#define INT_LOG_DEBUG(tag, fmt, ...)  Logger::debug(F("[%s][D] " fmt), tag, ##__VA_ARGS__)
#else
	#define INT_LOG_DEBUG(tag, fmt, ...)
#endif
#define INT_LOG_INFO(tag, fmt, ...)   Logger::info(F("[%s][I] " fmt), tag, ##__VA_ARGS__)
#define INT_LOG_WARN(tag, fmt, ...)   Logger::warn(F("[%s][W] " fmt), tag, ##__VA_ARGS__)
#define INT_LOG_ERROR(tag, fmt, ...)  Logger::error(F("[%s][E] " fmt), tag, ##__VA_ARGS__)
```
Integration code will use `const char* integrationTag()` providing a short token (e.g., "HA", "ANDROID"). Core code uses "CORE".
We can later add timestamp injection if logger lacks it: `millis()` prefix.

### Build Scaffolding (B1)
Add to `platformio.ini`:
```
[base_android]
extends = base
build_flags =
	${base.build_flags}
	-DANDROID_INTEGRATION  ; reserved macro (no code yet)
lib_deps =
	${base.lib_deps}
```
And optional `env:debugAndroid` / `env:releaseAndroid` mirror sections (commented out until implementation).

### Test Harness Concept (T1/T2)
Mock integration collects:
```
struct ReceivedEventLog {
	std::vector<String> phoneStateEvents;
	std::vector<String> callEvents;
	std::vector<String> systemEvents;
	std::vector<String> actionsTriggered;
};
```
Sequence test: simulate states in `State` struct → call `IntegrationManager::process()` ticks → assert ordering and counts.

---

## 8. Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| Enum refactor breaks existing HA code | Compile errors / runtime mismatch | Stage refactor with transitional aliases & incremental PRs |
| Increased flash usage (capabilities + handlers) | Might exceed target memory | Keep features behind `#ifdef MULTI_INTEGRATION_PREP` until stable |
| Action abstraction over-engineered if Android needs something different | Wasted complexity | Keep design minimal; implement only necessary methods now |
| Duplicate handler claims | Ambiguous user experience | Conflict detection (Task M3) |

---

## 9. Decision Log
| Date | Decision | Rationale | Status |
|------|----------|-----------|--------|
| 2025-09-06 | Adopt action handler registry vs per-integration parsing | Unified extensibility | Done |
| 2025-09-06 | Neutralize ConfigChangeEvent (remove HA specifics) | Prevent leakage & simplify multi-integration | Done |
| 2025-09-06 | Provide IntegrationService singleton option | Future RAM optimization | Done |
| 2025-09-06 | Add lightweight JSON logging (seq+ts) | Minimal overhead, tooling-friendly | Done |

---

## 10. Validation Plan
1. Compile with only HA integration: should build, no behavior change (baseline regression).  
2. Compile with reserved `ANDROID_INTEGRATION` macro (no Android sources) → Should still compile (capabilities exclude Android).  
3. Add MockIntegration (tests/harness) → verify broadcasting order.  
4. Simulate number code partial + full match path through new registry (unit test or serial debug script).  
5. Confirm config change events fire identically pre/post refactor (log diff).  

Success Metrics:
* Zero increase in call latency or missed state transitions (qualitative logs).  
* No HA code changes required beyond mechanical enum rename / deprecation wrappers.  
* Ability to compile with both HA + placeholder Android macros simultaneously.  

---

## 11. Definition of Done (Preparation Phase)
All MUST (all satisfied):
- [x] All A*, C*, B1 tasks complete & documented.
- [x] Manager contains no direct `HAIntegration` downcasts.
- [x] Action trigger path implemented; no `triggerWebhook` symbol remains in active code.
- [x] Config change enumeration refactor executed & validated.
- [x] Capability summary logged at startup.
- [x] Mock integration harness code merged.
- [x] Extension Guide authored.

Extended (Nice-to-haves) Achieved:
- [x] IntegrationService singleton option (N1).
- [x] Metrics snapshot export ('m').
- [x] Structured JSON logging with seq+timestamp ('j'/'k').

---

## 12. Next Phase Placeholder (Post-Preparation)
Will define once all preparation tasks marked [D]. Expected topics: Android transport channel selection, message framing, pairing workflow, security considerations, reconnection strategy, action mapping, packaging.

---

## 13. Progress Snapshot
Final Progress: Completed A1 A2 A3 A4 A5 B1 B2 C1 C2 C3 C4 T1 T2 T3 D1 D2 D3 M1 M2 M3 M4 N1 N2 N3. Preparation phase fully complete.

## 14. Post-Preparation State Summary
| Area | Status |
|------|--------|
| Actions | Generic handlers + conflict detection operational |
| Config Events | Neutral enum + INTEGRATION_EXTENSION_CHANGED |
| Logging | Structured macros + runtime toggle + JSON seq/timestamp logs |
| Extensibility | Dev Guide finalized; Android macro reserved |
| Observability | Metrics snapshot + stats/system events |
| Memory | IntegrationService singleton ready |

## 15. Sign-Off
Preparation Owner: (assign)  
Reviewer: (assign)  
Date: 2025-09-06  
Status: CLOSED – Ready for Android transport design phase.

