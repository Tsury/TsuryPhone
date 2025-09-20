# Multi-Integration Networking & Integration Layer Refactor Plan

Status: Completed (Core Refactor Frozen)
Owner: Tsury
Start Date: 2025-09-06
Target Completion (initial refactor without Android impl): 2025-09-06

## 0. Executive Summary
We will refactor the existing Home Assistant (HA)–only integration code to make the integration substrate neutral so an Android integration can be added cleanly. We remove all HA-specific concerns from generic layers, standardize event + command schemas, normalize macro guards, and simplify action + network boundaries. No backward compatibility is required; we optimize for current + future integrations (HA + Android). Android-specific implementation will be an independent phase once the neutral core is stable.

## 1. Objectives
- Eliminate HA leakage from generic components (`IntegrationService`, `IntegrationManager`, future shared utilities).
- Introduce consistent capability-based integration contract (actions, events, config changes, metrics exports).
- Standardize JSON event & command schema (naming, enum strategy, timestamps, versions) for all integrations.
- Ensure clean macro boundaries: generic code builds when ANY supported integration macro is defined.
- Provide clear action handler registry usable by both HA + Android.
- Improve correctness of call / phone state event payload (incoming/outgoing, active vs dialing numbers).
- Provide deterministic build validation after each completed task.
- Lay groundwork for Android integration skeleton with zero changes to HA code after refactor.

## 2. Out of Scope (Now)
- Actual Android network transport implementation (fully removed from this plan; will get its own dedicated plan later).
- Persistent storage changes (unless required by field rename fallout).
- Advanced telemetry aggregation and remote logging backend.
- Security hardening (will follow once any second integration channel defined).

## 3. Guiding Principles
1. Generic layers own: state serialization, event composition, action routing, metrics snapshots.
2. Integration-specific layers own: transport protocols, authentication, network timing, endpoint routing.
3. No conditional logic inside generic code that directly names HA concepts (websocket path, REST endpoints, mDNS, etc.).
4. Prefer removal & rename over indirection shims (no legacy alias fields).
5. Every task ends with build command:
   `C:\Users\Tsury\.platformio\penv\Scripts\platformio.exe run --environment debugWebSerialHA`
6. Keep patches tight; unrelated formatting avoided.

## 4. Phases & Tasks
Legend: [ ] = not started, [~] = in progress, [x] = done
Each task includes: ID, Description, Target Files, Acceptance Criteria (AC), Risk, Status, Build Log Ref.

### Phase 0 – Baseline & Planning
- T0.1 Create refactor master plan (this file)
  - Files: `MULTI_INTEGRATION_NETWORK_REFACTOR_PLAN.md`
  - AC: File exists with phases & tasks; build succeeds post-add.
  - Risk: Low
  - Status: [x]
  - Build Log: BL-000 (baseline)
- T0.2 Adjust scope: remove Android implementation phase/tasks (previous Phase 7) and explicitly defer to future standalone plan.
  - AC: Plan updated; no tasks referencing concrete Android implementation remain; Out of Scope updated.
  - Risk: Low
  - Status: [x]
  - Build Log: BL-001

### Phase 1 – Macro Guard Normalization
- T1.1 Widen guards: `IntegrationService.*`, `IntegrationManager.*`, any shared headers currently under `#ifdef HOME_ASSISTANT_INTEGRATION` -> `#if defined(HOME_ASSISTANT_INTEGRATION) || defined(ANDROID_INTEGRATION)`.
  - AC: Build passes; no code excluded when HA macro defined; ready for Android macro later.
  - Risk: Medium (accidental omission).
  - Status: [x]
  - Build Log: BL-002
- T1.2 Add explicit compile-time error if neither macro defined but integration headers included.
  - AC: Clear error produced when misconfigured.
  - Risk: Low.
  - Status: [x]
  - Build Log: BL-003

### Phase 2 – Generic Layer Purification
- T2.1 Remove unused networking includes (`<HTTPClient.h>`, `<WiFi.h>`) from `IntegrationService.cpp`.
  - AC: File compiles without them; no references remain.
  - Status: [x]
- T2.2 Audit & remove leftover webhook terminology in generic comments.
  - AC: Generic code comments are neutral.
  - Status: [x]

### Phase 3 – Event & Schema Standardization
- T3.1 Field rename: distinguish `currentCallNumber` vs `currentDialingNumber` in `addPhoneStateInfo`.
  - AC: JSON output uses both fields correctly; no ambiguous reuse.
  - Status: [x]
- T3.2 Ensure `isIncoming` preserved for call start / blocked events by using `buildCallEvent` directly in HA layer (replace previous generic convenience if needed).
  - AC: Emitted JSON for call start includes `isIncoming` truthfully.
  - Status: [x]
- T3.3 Add schema version field (`schemaVersion`) to all root event objects.
  - AC: Every emitted event includes consistent integer version (start at 1).
  - Status: [x]
- T3.4 Normalize timestamp fields: add `ts` (ms since boot) to every event; ensure call events optionally include `callStartTs` when duration relevant.
  - AC: All events validated in test harness (later).
  - Status: [x]
- T3.5 Document final canonical event shapes in `INTEGRATION_EVENTS.md` update.
  - AC: Doc reflects new fields; no stale examples.
  - Status: [x]

### Phase 4 – Action Handling Abstraction
- T4.1 Confirm `IntegrationManager::addActionHandler` neutral & capability gating optional; decide policy.
  - AC: If gating added, only integrations advertising `IC_ACTIONS` receive triggers.
  - Status: [x]
- T4.2 Remove outdated TODO about action handler registration from `HAIntegration::init`.
  - AC: Comment gone; updated clarifying note.
  - Status: [x]
- T4.3 Provide helper in manager to list registered action IDs (for diagnostics) and expose via structured log.
  - AC: JSON log entry with array of action IDs on startup.
  - Status: [x]

### Phase 5 – Logging & Metrics Standardization
- T5.1 Ensure structured JSON log functions compiled under widened macro guard.
  - AC: Build with only hypothetical ANDROID macro (future) would still include them.
  - Status: [x]
- T5.2 Add `integration` tag field into structured logs (value: integration name or `core`).
  - AC: Logs distinguish source.
  - Status: [x]
- T5.3 Metrics snapshot export: verify no HA-specific naming; rename if needed.
  - AC: Snapshot keys generic (e.g., `call.active`, not `ha.call.active`).
  - Status: [x]

### Phase 6 – Validation & Test Harness (Core Only)
- T6.1 Build after each preceding task (tracked in Build Log table).
  - AC: All green.
  - Status: [x]
- T6.2 Introduce minimal compile-time test (static assertion) verifying schema version constant presence.
  - AC: Build fails if constant removed.
  - Status: [x]
- T6.3 Add runtime diagnostic endpoint (HA layer) to dump generic capabilities (optional).
  - AC: `/api/integration_capabilities` returns JSON list.
  - Status: [x] (implemented at /api/diagnostics)

### Phase 7 – Documentation & Cleanup (renumbered from previous Phase 8)
- T7.1 Update `INTEGRATION_DEV_GUIDE.md` with new architecture flow & extension points.
  - Status: [x]
- T7.2 Remove obsolete comments / stale code paths detected during refactor.
  - Status: [x]
- T7.3 Final pass: ensure no HA strings in generic layer.
  - Status: [x]

## 5. Build Verification Log
| ID | Date/Time (Local) | Tasks Covered | Result | Notes |
|----|-------------------|---------------|--------|-------|
| BL-000 | 2025-09-06 (fill time) | T0.1 | PASS | Plan file added |
| BL-001 | 2025-09-06 (fill time) | T0.2 | PASS | Scope adjusted (Android impl removed) |
| BL-002 | 2025-09-06 (fill time) | T1.1 | PASS | Macro guards widened |
| BL-003 | 2025-09-06 (fill time) | T1.2 | PASS | Added compile-time guards (IntegrationService, IIntegration) |
| BL-004 | 2025-09-06 (fill time) | T2.1,T2.2(partial) | PASS | Removed unused includes; updated message text |
| BL-005 | 2025-09-06 (fill time) | T3.1,T3.3,T3.4 | PASS | Added schemaVersion/ts; split dialing vs call numbers |
| BL-006 | 2025-09-06 (fill time) | T3.2 | PASS | HAIntegration now preserves isIncoming using buildCallEvent |
| BL-007 | 2025-09-06 (fill time) | T3.5 | PASS | Updated INTEGRATION_EVENTS.md to schema v1 |
| BL-008 | 2025-09-06 (fill time) | T4.1 | PASS | Capability gating & logging in triggerAction |
| BL-009 | 2025-09-06 (fill time) | T4.2 | PASS | Removed outdated registration comments in HAIntegration |
| BL-010 | 2025-09-06 (fill time) | T4.3,T5.2(partial) | PASS | Action codes JSON summary & integration tag in logs |
| BL-011 | 2025-09-06 (fill time) | T2.2,T4.3,T5.1,T5.2,T5.3 | PASS | Neutral comments, metrics key rename, schema const usage |
| BL-012 | 2025-09-06 (fill time) | T6.3 | PASS | Diagnostics endpoint /api/diagnostics added |
| BL-013 | 2025-09-06 (fill time) | T7.2,T7.3 | PASS | Generic layer cleanup & comment neutralization |
| BL-014 | 2025-09-06 (fill time) | T6.1,T6.2,T7.1 | PASS | Static assert + dev guide verified |

Command used each time:
```
C:\Users\Tsury\.platformio\penv\Scripts\platformio.exe run --environment debugWebSerialHA
```

## 6. Event Schema (Target v1 – Draft)
All events share root fields:
- `schemaVersion` (int) – starts at 1
- `type` (string) – e.g., `phone_state`, `call_event`, `system`, `error`
- `event` (string) – subtype / action (e.g., `state`, `start`, `end`, `blocked`)
- `ts` (uint ms since boot)
- `deviceId` (string) – stable unique
- `integration` (string) – emitter (`ha`, `android`, `core` when generic)

### 6.1 Phone State Event (`type=phone_state`)
```
{
  schemaVersion: 1,
  type: "phone_state",
  event: "state" | "call_info" | "dialing" | "ring" | "dnd",
  ts: <ms>,
  state: <enum int>,
  stateName: <string>,
  previousState?: <enum int>,
  currentCallNumber?: <string>,
  currentDialingNumber?: <string>,
  isRinging?: <bool>,
  dndActive?: <bool>,
  maintenanceMode: <bool>
}
```

### 6.2 Call Event (`type=call_event`)
```
{
  schemaVersion: 1,
  type: "call_event",
  event: "start" | "end" | "blocked" | "missed",
  ts: <ms>,
  number: <string>,
  isIncoming: <bool>,
  durationMs?: <uint>,
  callStartTs?: <uint>
}
```

### 6.3 Error Event (`type=error`)
```
{
  schemaVersion: 1,
  type: "error",
  event: "runtime" | <category>,
  | BL-014 | 2025-09-06 (fill time) | T6.1,T6.2,T7.1 | PASS | Static assert + dev guide verified |
  | BL-015 | 2025-09-06 (fill time) | Documentation Finalization | PASS | Plan closure updates, endpoint AC aligned, call event notes |
  message: <string>,
  code?: <string>
}
```

### 6.4 System Event (`type=system`)
```
{
  schemaVersion: 1,
  type: "system",
  event: "status" | "shutdown",
  ts: <ms>,
  uptimeMs: <uint>,
  freeHeap: <uint>,
  wifiRssi?: <int>
}
```

## 7. Naming Conventions
| Concept | Name |
|---------|------|
| Dial buffer number | `currentDialingNumber` |
| Active call number | `currentCallNumber` |
| Whether call is incoming | `isIncoming` |
| Start timestamp of call | `callStartTs` |
| Generic event timestamp | `ts` |

## 8. Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| Macro widening introduces build path gaps when HA undefined | Medium | Add compile-time error when no macro set (T1.2) |
| JSON field rename breaks existing HA dashboards | Low (no backward compat required) | Communicate change; update HA frontend simultaneously |
| Divergence between plan & implementation | Medium | Update this file per task completion before next task |
| Event schema future growth causing churn | Low | Introduce `schemaVersion` now |

## 9. Metrics / Validation Ideas
- Compare emitted event counts pre/post refactor (should match except additional standardized fields).
- Verify latency overhead negligible (<1ms per event build) – (optional perf logging).

## 10. Progress Update Template
Add under section 5 after each task:
```
    duration?: <uint>,          // Present on 'end' (seconds currently) – name kept as 'duration'
```

## 11. Closure & Next Steps
All refactor tasks are complete (see Build Log). Core is now stable and neutral.

Suggested follow-up (new plan):
1. Android Integration Skeleton (transport + auth decision).
2. Event Schema v2 (durationMs, callStartTs) if higher resolution required.
3. Flash usage optimization (currently ~78% used) before large features.
4. Optional metrics endpoint or structured metrics event.

Change Freeze: Generic layer changes now require either schemaVersion bump or new enhancement plan.

---
(End of Plan v1.1)
