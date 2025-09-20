# Integration Test Harness Guide (Preparation Phase)

Status: Draft (Preparation Infra Complete)

This document describes a lightweight manual/serial-based harness approach for validating multi-integration behavior before a full automated test framework is introduced.

## Goals
- Exercise `IntegrationManager` state propagation paths deterministically.
- Verify action handler resolution (quick dial vs integration action codes).
- Capture emitted events without network dependencies via `MockIntegration`.
- Provide reproducible manual steps using PlatformIO + WebSerial.

## Components
| Item | Purpose |
|------|---------|
| `MockIntegration` | Captures phone state, call, system, and action events in ring buffers (16 entries each). |
| `IntegrationManager::listActionCodes()` | Debug log listing of registered action codes after initialization. |
| Logging Macros | Uniform, tag-based filtering. Use `CORE`, `HA`, `MOCK`. |

## Enabling the Harness
Build with the mock integration flag:
```
platformio run -e debugWebSerialHAMock
```
This environment adds `-DMOCK_INTEGRRATION` (typo corrected below) enabling `MockIntegration` alongside Home Assistant.

If adding the macro manually:
```
platformio run -e debugWebSerialHA --project-option "build_flags=${env.build_flags} -DMOCK_INTEGRATION"
```

## Event Capture Strategy
`MockIntegration` stores compact textual encodings:
- Phone state transitions: `state:<prev>-><new>`
- Dial progress: `dial:<digits>`
- Ring state: `ring:0|1`
- DND changes: `dnd:0|1`
- Call info: `call_info:IN:<num>` or `call_info:OUT:<num>`
- Call lifecycle: `start:IN:<num>`, `end:<duration>`, `blocked:<num>`
- Actions: raw action id recorded
- Config changes: `config_change`
- System status tick: `system:status`

(These can later be exported over a diagnostic endpoint or serialized via a future debug command.)

## Manual Test Sequences
1. Power on with mock env; confirm log lines:
   - `[CORE][I] Registered Mock integration`
   - `[CORE][I] Capabilities for MOCK: ` (none expected)
2. Dial an action code (one of the Home Assistant webhook codes). Expect:
   - `[CORE][I] Action trigger <id>`
   - `[MOCK][I] (action captured as entry in actions buffer)`
3. Dial a quick dial code; ensure it does NOT appear as action (unless overlapping code configured) and produces dialing + call events.
4. Simulate incoming call (manually toggle state via existing system or by injecting state if helper added in future) and verify `call_info`, `start`, `end` ordering.
5. Toggle DND (via HA command or direct config) – expect `dnd:` entry and config change broadcast.

## Validation Checklist
| Aspect | Expected |
|--------|----------|
| State transitions | Exactly one `state:` entry per actual app state change |
| Action trigger | Appears in `actions` buffer and not mis-classified as quick dial |
| Conflict warnings | None (unless intentionally introducing duplicate codes) |
| Logging toggle | `IntegrationManager::enableIntegrationDebugLogging(false)` silences `INT_LOG_DEBUG` lines |

## Extending Harness
- Add a serial command to dump buffers (future task, optional).
- Add JSON structured log channel (deferred N3).
- Wrap buffers with a minimal circular index + timestamp for later replay.

## Future Automation Path
1. Introduce a test-only shim that feeds synthetic `State` transitions into `IntegrationManager::process()` without hardware input.
2. Expose buffer snapshot function returning a compact JSON for host-driven assertions.
3. Add host script (Python) invoking PlatformIO unit test or remote serial session to drive deterministic sequences.

## Known Limitations
- No automatic flushing/dumping: must rely on log instrumentation for now.
- Buffers are in-memory only; power cycle loses data.
- No concurrency / race simulation (single-threaded loop only).

---
Update this file when: buffer formats change, new integration capabilities added, or automated harness introduced.
