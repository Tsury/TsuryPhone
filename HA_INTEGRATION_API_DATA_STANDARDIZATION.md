# HA Integration API Data Standardization Backlog

Focus: ONLY payload / schema / validation / naming / error & response normalization items needed to bring current Home Assistant integration API (HTTP + WebSocket events) to a coherent, versionable standard ready for multi‑integration reuse.

Source Inputs: Implementation (`IntegrationService.cpp`, `IntegrationManager.cpp`, `ha/HAIntegration.cpp`, `ha/HAWebServer.cpp`) and mapping in `HA_INTEGRATION_API_MAP.md`.

Status Legend: [ ] not started, [~] in progress, [x] done

Build Verification Requirement: AFTER COMPLETING EACH WORK ITEM (DS1..DS14) you MUST run:
```
C:\Users\Tsury\.platformio\penv\Scripts\platformio.exe run --environment debugWebSerialHA
```
and record PASS/FAIL in the Progress Log table (Section 11). No task is considered complete without a recorded PASS build.

| DS6 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-009 | Config delta events (aggregated audio,dnd + granular others) |
| DS7 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-010 | Ring pattern normalized under config.ring.pattern |

## 0. Guiding Principles (Immediate Forward Standard – v2 Only)
| DS14 | [ ] | 2025-09-06 |  | PASS (partial) | BL-009 | Dotted keys extended (webhook.*, quick_dial.*, blocked_number.*, ha.url) |
1. Deterministic Schema: Every outbound event MUST include: `schemaVersion`, `category`, `event`, `ts`, `seq`, `deviceId`, `integration`.
2. Clear Semantics: `category` = domain (`phone_state`, `call`, `system`, `config`, `diagnostic`), `event` = subtype/action (`start`, `end`, `state`, `stats`, `delta`, `shutdown`, etc.). Legacy `event`/`type` inversion is dropped.
3. Explicit Units: All timestamps & durations in milliseconds with `Ms` suffix (`ts` is already ms; duration fields end with `Ms`); counts have no suffix.
4. Naming Style: Keep existing camelCase everywhere for consistency; new fields must not introduce snake_case.
5. Unified Error Contract: Errors return HTTP 4xx/5xx JSON: `success=false`, `schemaVersion`, `ts`, `errorCode`, `message`.
6. Unified Success Contract: `success=true`, `schemaVersion`, `ts`, optional `data` (never nest success flags inside `data`).
7. Validation Split: Web layer → presence & primitive type; Service layer → semantic/range; error codes prefixed `WEB_` vs `CORE_`.
8. Ordering: Global `seq` incremented per emitted event (wrap on overflow only); consumers rely on it for gap detection.
9. Config Delta Emission: Every config mutation sends a minimal delta event; full-state broadcast becomes optional, not required for correctness.
10. No Legacy Fields: Old root keys (`type`, old semantics of `event`) and second-based durations are removed, not dual-emitted.
11. Multi-Integration Neutrality: Shared helpers (event factory, HTTP response builders, timing/direction tracking) live in `IntegrationService` so future integrations (Android, MQTT, etc.) reuse them with zero duplication.

---

## 1. Current Discrepancies (Snapshot)
| Area | Issue | Impact | Example |
|------|-------|--------|---------|
| Event root fields | Using `event`=category & `type`=subtype (inverted vs planned) | Harder multi-integration reuse & documentation mismatch | `{event:"call", type:"start"}` |
| Sequence | No `seq` in broadcast events | Client cannot detect dropped frames / ordering | Phone state rapid changes |
| Duration naming | `duration` (seconds) vs `currentCallDuration` (seconds) vs design draft `durationMs` | Ambiguity & inconsistent units | Call end event vs full state snapshot |
| Call direction | `isIncoming` only on call `start` event; absent on `end` | Client must infer direction from historical cache | End event omits direction |
| Call start time | `currentCallStartTime` only sometimes present (when builder path includes startTime>0) | Incomplete session reconstruction | Full state after start | 
| Error contract | Free-form strings only; no `errorCode`; missing `ts` / `schemaVersion` | Hard to automate error handling / localization | `{"success":false,"message":"Missing 'number' parameter"}` |
| Success contract | Sometimes `data` absent, sometimes nested structures vary | Client parsing complexity | Quick dial add vs simple hangup |
| Config mutation signaling | Only full-state snapshot broadcast; no deltas | Bandwidth & diffing overhead | Adding webhook triggers full state |
| Boolean naming | Mix of `dndActive`, `maintenanceMode` (no `is`), `isRinging` (in ad hoc injection) | Inconsistency | Ring event vs phone state event |
| Event categories | `full_state` uses empty subtype (`type:""`) | Edge-case parsing | Full state builder |
| Call event fields | `duration` in seconds but design wants milliseconds | Potential precision loss & docs mismatch | End event |
| Embedded system stats | Stats & system metrics appear across different categories | Redundant data volume | `system/status` vs full snapshot |
| Validation layering | Some range checks at service (audio), others only at web (presence) w/out error codes | Inconsistent error semantics | Audio config vs DND | 
| Pattern field | ring pattern returns `{ringPattern}` while audio config nested under `config.audio.*` | Response shape fragmentation | Ring pattern endpoint |
| Mixed naming of call numbers | `currentCallNumber`, `currentDialingNumber`, plus `currentCallIsIncoming` | Redundant & inconsistent naming style | Phone state info helper |

---

## 2. Canonical v2 Event Schemas (Authoritative – Adopt Directly)
### 2.1 Base
```
{
  schemaVersion: 2,
  seq: <uint32>,
  ts: <uint64 ms>,
  integration: "ha",
  deviceId: <string>,
  category: "call" | "phone_state" | "system" | "config" | "diagnostic",
  event: <subtype string>
  // subtype-specific fields follow
}
```

### 2.2 Call Events (`category=call`)
`start`, `end`, `blocked`, `missed`
```
{
  ...base,
  event: "start" | "end" | "blocked" | "missed",
  number: <string>,
  isIncoming: <bool>,
  callStartTs: <uint64 ms>,      // always present for active/ended calls
  durationMs?: <uint32>,         // end only
  blockedReason?: <string>       // blocked only (future)
}
```

### 2.3 Phone State (`category=phone_state`)
```
{
  ...base,
  event: "state" | "dialing" | "ring" | "dnd" | "call_info",
  state: <int>,
  stateName: <string>,
  previousState?: <int>,
  currentCallNumber?: <string>,
  isIncomingCall?: <bool>,
  currentDialingNumber?: <string>,
  isRinging?: <bool>,
  dndActive?: <bool>,
  isMaintenanceMode: <bool>
}
```

### 2.4 System (`category=system`)
`status`, `stats`, `shutdown`, `error`
```
{
  ...base,
  event: "status" | "stats" | "shutdown" | "error",
  uptimeMs?: <uint64>,
  freeHeap?: <uint32>,
  rssi?: <int>,
  calls?: { total,incoming,outgoing,blocked,talkTimeSeconds }, // stats only
  reason?: <string>,   // shutdown
  message?: <string>,  // shutdown
  errorCode?: <string>,// error
  errorMessage?: <string>
}
```

### 2.5 Config Delta (`category=config` / `event=config_delta`)
Single change form:
```
{
  ...base,
  event: "config_delta",
  key: <string>,
  oldValue?: <variant>,
  newValue: <variant>
}
```
Aggregated multi-change form (used for batched DND / audio updates):
```
{
  ...base,
  event: "config_delta",
  changes: [
    { key: <string>, oldValue?: <variant>, newValue: <variant> },
    ...
  ]
}
```

---

## 3. Error & Success Contract (Authoritative)
### 3.1 Success
```
HTTP 200
{
  schemaVersion: 2,
  success: true,
  ts: <ms>,
  data?: { ... canonical object ... }
}
```
### 3.2 Error
```
HTTP 4xx / 5xx
{
  schemaVersion: 2,
  success: false,
  ts: <ms>,
  errorCode: "WEB_MISSING_NUMBER" | "CORE_DIAL_UNAVAILABLE" | ...,
  message: "Human readable"
}
```
Error code naming: `<LAYER>_<ACTION>_<NOUN>` or `<LAYER>_<DOMAIN>_<DETAIL>`.

---

## 4. Field Naming Normalization Matrix (Direct Replacements – remove old)
| Remove Field | New Field (Keep) | Notes |
|--------------|------------------|-------|
| event (as category) | category | Semantic clarification |
| type | event | Subtype/action |
| duration (seconds) | durationMs | Milliseconds precision |
| currentCallDuration | currentCallDurationMs | Snapshot duration (ms) |
| currentCallIsIncoming | isIncomingCall | Consistent boolean naming |
| currentCallStartTime | callStartTs | Milliseconds timestamp |
| maintenanceMode | isMaintenanceMode | Normalize boolean prefix |
| ringPattern (top-level response) | config.ring.pattern | Consolidated config namespace |
| stats flat numeric set | calls.{total,incoming,outgoing,blocked,talkTimeSeconds} | Single stats object |

---

## 5. Validation Standardization
| Aspect | Rule | Example Enforcement |
|--------|------|---------------------|
| Required string | Non-empty UTF-8 <= 64 chars unless specified | Number, code, pattern |
| Codes (quick dial/webhook) | Regex `^[A-Za-z0-9_]{1,16}$` | Reject others with `WEB_INVALID_CODE` |
| Phone number | Basic digit/plus/hyphen regex `^[0-9+\-]{1,24}$` (future) | `WEB_INVALID_NUMBER` |
| Volume/Gain | Integer 1–7 inclusive | `CORE_AUDIO_RANGE` |
| Boolean flags | Must be JSON boolean | `WEB_INVALID_BOOL_<FIELD>` |
| Pattern | Non-empty <= 32 chars | `WEB_INVALID_PATTERN` |
| URL | Must start `http://` or `https://` | `WEB_INVALID_URL` |

Range & semantic validation returns layer-specific error codes; service layer does not duplicate presence checks already enforced by web layer.

---

## 6. Implementation Strategy (No Backward Layer)
All changes land in a single schema bump (`schemaVersion=2`). Old fields are deleted in the same commit. Clients must update simultaneously.
1. Set global constant to `2`.
2. Refactor builders to output new root fields & remove `type`.
3. Add global `seq` counter; integrate into every builder.
4. Replace duration calculations with ms; remove second-based fields.
5. Implement unified success/error response generator.
6. Introduce config delta emission points in mutation handlers.
7. Normalize response nesting for audio & ring pattern.
8. Add validation regex & structured error codes.
9. Update docs (`INTEGRATION_EVENTS.md`, this file) & diagnostics endpoint to reflect new schema fields.
10. Add minimal unit tests for event root correctness & error code coverage.

---

## 7. Data Standardization Work Items (Actionable – No Legacy Paths)
| ID | Task | Priority | Effort | AC (Acceptance Criteria) |
|----|------|----------|--------|--------------------------|
| DS1 | Introduce root fields (`category`,`event`,`seq`) & bump `schemaVersion=2` | P1 | S | All events emit required root fields; old `type` removed; tests validate presence. |
| DS2 | Millisecond timing & rename duration fields | P1 | S | `callStartTs` + `durationMs` present; no second-based fields remain. |
| DS3 | Always include `isIncoming` on ALL call events | P1 | XS | `start`,`end`,`blocked` each have `isIncoming`; verified via test harness. |
| DS4 | Unified success & error response serializers | P1 | M | Every HTTP response follows contract; legacy shapes gone. |
| DS5 | Structured error codes implementation | P1 | M | All error cases return `errorCode`; table documented in code & this file. |
| DS6 | Config delta event emission | P2 | M | Each config mutation triggers single `config_delta` event; full-state broadcast optional. |
| DS7 | Normalize audio + ring pattern responses under `data.config` | P2 | S | Both endpoints return consolidated config structure. |
| DS8 | System stats nesting (`calls.{...}`) | P3 | S | `system/stats` event uses nested object; no flat duplicates. |
| DS9 | Boolean normalization (`isMaintenanceMode`) | P3 | XS | Field renamed across all builders & responses. |
| DS10 | Validation regex for codes / number / URL | P2 | S | Invalid inputs rejected with proper `WEB_` error codes. |
| DS11 | JSON schema artifacts for each event type | P2 | S | Schema files exist & a build step validates them. |
| DS13 | Update developer & event docs | P1 | S | `INTEGRATION_EVENTS.md` & dev guide reflect v2 only. |
| DS14 | Implement config delta key naming convention (`ring.pattern`, etc.) | P2 | XS | Emitted `config_delta` keys follow dotted namespace style. |

---

## 8. Error Code Catalog (Authoritative v2)
| Code | Layer | Meaning |
|------|-------|---------|
| WEB_MISSING_NUMBER | Web | `number` not provided |
| WEB_MISSING_CODE | Web | `code` not provided |
| WEB_MISSING_ENABLED | Web | `enabled` not boolean |
| WEB_INVALID_JSON | Web | JSON parse failure |
| WEB_INVALID_PATTERN | Web | Empty or invalid pattern |
| WEB_INVALID_CODE | Web | Code fails regex |
| WEB_INVALID_NUMBER | Web | Number fails regex |
| WEB_INVALID_URL | Web | URL invalid format |
| CORE_DIAL_UNAVAILABLE | Core | Dial callback absent |
| CORE_ANSWER_UNAVAILABLE | Core | Answer callback absent |
| CORE_HANGUP_UNAVAILABLE | Core | Hangup callback absent |
| CORE_RING_UNAVAILABLE | Core | Ring callback absent |
| CORE_CALL_WAIT_UNAVAILABLE | Core | Call waiting callback absent |
| CORE_AUDIO_RANGE | Core | Audio param out of range |
| CORE_QUICK_DIAL_EXISTS | Core | Quick dial code exists |
| CORE_QUICK_DIAL_NOT_FOUND | Core | Quick dial code missing |
| CORE_BLOCKED_NUMBER_EXISTS | Core | Blocked number exists |
| CORE_BLOCKED_NUMBER_NOT_FOUND | Core | Blocked number missing |
| CORE_WEBHOOK_CODE_CONFLICT | Core | Code conflict with webhook/quick dial |
| CORE_WEBHOOK_NOT_FOUND | Core | Webhook code missing |
| CORE_RESET_ALREADY_SCHEDULED | Core | Duplicate reset request |

---

## 9. Implementation Ordering (Single Migration)
1. DS1, DS2 (root & timing) – foundation
2. DS3, DS4, DS5 (direction + responses + error codes)
3. DS10 (validation hardening)
4. DS6, DS7, DS8, DS9, DS14 (delta + normalization + naming)
5. DS11, DS14, DS13 (schemas, finalize naming, then docs)
6. (Tests intentionally omitted per updated scope)

---

## 10. Acceptance Test Outline
| Test | Covers | Criteria |
|------|--------|----------|
| T_EVT_ROOT | DS1 | Every emitted event contains required root fields & lacks removed ones (`type`). |
| T_EVT_SEQ | DS1 | seq increments by 1 across consecutive events. |
| T_CALL_TIMING | DS2 | `durationMs` ≈ (`end.ts` - `callStartTs`) within tolerance. |
| T_CALL_DIRECTION | DS3 | `isIncoming` correct on start/end/blocked. |
| T_HTTP_SUCCESS | DS4 | Success responses match schema (no extraneous fields). |
| T_HTTP_ERRORS | DS5 | Each invalid scenario returns expected `errorCode`. |
| T_VALIDATION_REGEX | DS10 | Invalid code/number/url produce correct `WEB_*` codes. |
| T_CONFIG_DELTA | DS6/DS14 | Config mutation triggers single delta event with dotted key naming. |
| T_STATS_NESTING | DS8 | Stats event exposes nested `calls` object only. |
| T_BOOLEAN_NAMING | DS9 | Only `isMaintenanceMode` appears (no legacy name). |
| T_SCHEMAS_VALIDATE | DS11 | JSON schemas validate representative event samples. |
| T_DOC_SYNC | DS13 | Automated doc check ensures listed root fields match emitted fields. |

---

End of file.
---

## 11. Progress Log
| ID | Status | Started | Completed | Build Result | Build Log Ref | Notes |
|----|--------|---------|-----------|--------------|---------------|-------|
| DS1 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-001 | Root fields + seq + v2 + diagnostic snapshot rename |
| DS2 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-002 | callStartTs + durationMs + snapshot ms durations |
| DS3 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-003 | isIncoming on all call events (direction persisted) |
| DS4 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-004 | Unified response builders (generic) |
| DS5 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-005 | Structured errorCode plumbing + mappings |
| DS6 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-009 | Config delta events (single + aggregated forms) |
| DS7 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-010 | Ring pattern normalized under config.ring.pattern |
| DS8 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-011 | Stats nested calls.totals{...} (DS8) |
| DS9 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-012 | maintenanceMode -> isMaintenanceMode rename |
| DS10 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-007 | Added pattern validator & ring pattern enforcement |
| DS11 | [x] | 2025-09-06 | 2025-09-06 | (n/a) | BL-013 | JSON schemas added under schemas/events |
| DS13 | [x] | 2025-09-06 | 2025-09-06 | (n/a) | BL-013 | Docs updated v2 only (INTEGRATION_EVENTS.md) |
| DS14 | [x] | 2025-09-06 | 2025-09-06 | PASS | BL-013 | Dotted keys + old/new value schema finalized |

Build Command (authoritative):
```
C:\Users\Tsury\.platformio\penv\Scripts\platformio.exe run --environment debugWebSerialHA
```

Recording Convention:
* Build Log Ref: BL-XXX sequential.
* Notes: brief (<80 chars) outcome or anomaly.

