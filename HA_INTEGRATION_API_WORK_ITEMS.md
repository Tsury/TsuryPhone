# HA Integration API – Derived Work Items & Improvement Backlog

Source Basis: `HA_INTEGRATION_API_MAP.md` (auto‑generated communication map) and current implementation in: `IntegrationService`, `IntegrationManager`, `HAIntegration`, `HAWebServer`.

Goal: Track concrete, actionable improvements to bring implementation in line with design intentions, harden reliability/security, and prepare for multi‑integration future (Android, etc.). Each item lists: ID, Title, Category, Priority (P1=High, P2=Med, P3=Low), Effort (S/M/L), Dependencies, Rationale (from gaps/divergences), and Acceptance Criteria (AC).

Legend:  Schema = payload shape, Consistency = naming/unit parity, Observability = monitoring/logging, Reliability = resilience/robustness, Security = auth/hardening, DevEx = developer experience.

---

## 1. Schema & Consistency

| ID | Title | Cat | P | Effort | Dependencies | Rationale | Acceptance Criteria |
|----|-------|-----|---|--------|--------------|-----------|--------------------|
| S1 | Align root event field semantics (`event` vs `type`) | Schema | P1 | M | None (requires version bump) | Design doc expects `type=category`, `event=subtype`; code reversed. | New schemaVersion=2; events use `type` for category (`call`,`phone_state`,`system`,`full_state`), `event` for subtype (`start`,`state`, etc.). Old fields retained ONLY if a compatibility flag is on (optional); tests updated. |
| S2 | Standardize call duration units & naming | Consistency | P1 | S | S1 (if renaming fields, coordinate) | Mixed: `duration` (seconds), `currentCallDuration` (seconds), spec draft uses `durationMs`. | Adopt milliseconds: fields `durationMs` (call end), `currentCallDurationMs`; remove legacy names or keep behind compile flag; builders updated; tests validate numeric increase over time. |
| S3 | Ensure `isIncoming` present on all call events | Consistency | P1 | S | None | Currently only on start (and implicitly for blocked). | `isIncoming` included & correct on `start`, `end`, `blocked`, plus in full state snapshot when call active; unit tests simulate incoming/outgoing sequences. |
| S4 | Introduce `callStartTs` (ms) consistently | Schema | P2 | S | S2 | Start time only sometimes present (`currentCallStartTime`). | All call events & snapshots include `callStartTs` when a call active; removed `currentCallStartTime`; documentation updated. |
| S5 | Add monotonic sequence id to every emitted event | Schema | P2 | S | None | Ordering across WS frames currently implicit by arrival. | Field `seq` increments per event; never resets until restart; tests assert monotonic increase. |
| S6 | Add explicit `source` field (integration tag) in each payload | Schema | P2 | S | None | Design doc suggests integration tag; currently only structured logs have. | All events include `integration` (already) and optionally `source` if multi-layer (e.g., `core` vs `ha`). Decide if redundant; if retained, doc updated. |
| S7 | Normalize full-state event subtype | Consistency | P3 | XS | S1 | `full_state` uses empty subtype. | Provide subtype `snapshot` (e.g., `event=full_state`,`type=snapshot`) or rename category; schema doc updated. |
| S8 | Emit explicit config change delta event | Schema | P2 | M | Config mutation paths | Currently only full state broadcast; no small diff for config changes. | New event: category `system`, subtype `config_delta` with `{ changedKey, ts, oldValue?, newValue }`; fired for each config mutation. |

## 2. Observability & Diagnostics

| ID | Title | Cat | P | Effort | Dependencies | Rationale | Acceptance Criteria |
|----|-------|-----|---|--------|--------------|-----------|--------------------|
| O1 | WebSocket command ACK / error frames | Observability | P1 | M | None | Currently silent; client must poll HTTP for status. | For each inbound WS command, device sends `{command,id,status,success[,error][,data]}`; includes latency measurement server-side. |
| O2 | Structured error codes | Observability | P2 | S | O1 (optional) | Errors are free‑form strings; hard to automate. | Add `errorCode` stable identifier (e.g., `ERR_MISSING_NUMBER`); documentation table; tests cover mapping. |
| O3 | Metrics snapshot event unification | Observability | P3 | S | S2 | Call stats fields sometimes embedded vs separate. | `system/stats` carries `calls:{total,incoming,outgoing,blocked,talkTimeSeconds}` exactly; remove duplicates. |
| O4 | Emit ring state explicit off event | Observability | P2 | S | None | Only on change to ring on? off path reliant on generic state event. | Broadcast `phone_state/ring` with `isRinging:false` when ring ends. |
| O5 | Add `/health` endpoint (lightweight) | Observability | P2 | S | None | Health check currently requires heavier endpoints. | `GET /health` returns `200` + `{uptime, heap, schemaVersion}` minimal. |
| O6 | Diagnostics to list current action codes vector | Observability | P3 | S | None | Currently only logged at startup. | `/api/diagnostics` includes `actionCodes:[...]`. |

## 3. Reliability & Resilience

| ID | Title | Cat | P | Effort | Dependencies | Rationale | Acceptance Criteria |
|----|-------|-----|---|--------|--------------|-----------|--------------------|
| R1 | Outbound webhook retry & backoff | Reliability | P1 | M | None | Single POST attempt loses transient events. | Implement retry (e.g., 2 retries with exponential backoff); logs each attempt; aborts on non-retryable codes (4xx). |
| R2 | Ring operation debounce / rate limiting | Reliability | P2 | S | None | Possible abuse via repeated `/api/system/ring`. | Minimum interval guard (e.g., 500ms) else reject with `ERR_RING_RATE_LIMIT`; configurable constant. |
| R3 | Protect reset scheduling re-entrancy | Reliability | P3 | XS | None | Multiple reset requests could stack; guard variable exists but no rejection message. | Second reset request returns error `ERR_RESET_ALREADY_SCHEDULED`. |
| R4 | Validate quick dial & webhook code char set | Reliability | P3 | S | None | Currently any string accepted; risk of reserved delimiters later. | Regex validation (e.g., `^[A-Za-z0-9_]{1,16}$`); reject others. |

## 4. Security

| ID | Title | Cat | P | Effort | Dependencies | Rationale | Acceptance Criteria |
|----|-------|-----|---|--------|--------------|-----------|--------------------|
| SEC1 | Optional API token auth (header or query) | Security | P1 | M | None | All endpoints open. | Config flag enabling token; when enabled, all modifying endpoints require `X-API-Key`; unauthorized returns 401 JSON error. |
| SEC2 | Limit CORS origins (configurable) | Security | P2 | S | SEC1 optional | Current wildcard `*`. | Config field `allowedOrigins`; sets Access-Control headers; default can remain `*` if not set. |
| SEC3 | WebSocket origin / token enforcement | Security | P2 | M | SEC1 | Ensure WS clients also authenticate; drop unauthorized connection. | Connection rejected with code & log reason. |
| SEC4 | Sensitive field redaction in logs | Security | P3 | S | None | Future tokens could leak. | Token never printed in logs; test ensures masked. |

## 5. Developer Experience / Tooling

| ID | Title | Cat | P | Effort | Dependencies | Rationale | Acceptance Criteria |
|----|-------|-----|---|--------|--------------|-----------|--------------------|
| D1 | Auto-generated API docs from annotated sources | DevEx | P2 | M | Completion of S1–S4 | Manual doc drift risk. | Script parses annotations & regenerates API map + schema JSON; CI diff check. |
| D2 | JSON schema files for each event type | DevEx | P2 | S | S1,S2 | Enables validation & tooling. | `schemas/` directory with `.json` per event; validated in build (static test). |
| D3 | Unit tests for each command validation path | DevEx | P1 | M | Test harness available | Guard regressions for error strings & codes. | Tests assert success & failure permutations (boundary volumes 0,8 etc.). |
| D4 | Integration test: simulated call lifecycle | DevEx | P2 | M | D3 | Validate event ordering & field consistency. | Test asserts: start -> state -> end events with monotonic seq & correct isIncoming. |
| D5 | Lint rule / static assert for schema version presence | DevEx | P3 | XS | Existing static assert base | Reinforce schema constant usage for new builders. | Build fails if builder omits `schemaVersion`. |
| D6 | Mock integration harness parity with HA | DevEx | P3 | S | None | Keep mock path up to date. | Mock integration emits same schema fields for generic events. |

## 6. Migration Plan Outline (for S1–S4)
1. Introduce new fields (additive) while keeping legacy ones for one interim release: `schemaVersion=1` still default; if `ENABLE_SCHEMA_V2` compile flag set, emit both sets.
2. Provide compatibility macro gating dual field emission.
3. After validation, bump `INTEGRATION_EVENT_SCHEMA_VERSION` to 2 and drop legacy duplicates.
4. Update documentation + schemas + examples; run D3/D4 tests.

## 7. Prioritized Immediate Sprint Candidates
Recommended initial batch (max risk reduction & alignment): S1, S2, S3, O1, R1, SEC1, D3.

## 8. Open Questions
| Q | Topic | Action |
|---|-------|--------|
| Q1 | Should we keep both `event` & `type` names or rename to `category` / `action`? | Decide before S1 implementation. |
| Q2 | Duration unit: milliseconds vs seconds? | Lean ms for precision; confirm storage & bandwidth impact negligible. |
| Q3 | WS ACK payload shape includes original `data` echo? | Likely include truncated echo (e.g., first 64 chars) or hash for large bodies. |
| Q4 | API token storage persistence? | Add to config + export hashed in diagnostics only. |

## 9. Tracking Template (for execution)
Copy for each item when moving to progress log:
```
ID: <ID>
Status: TODO / WIP / DONE
Owner: <name>
Started: <date>
Merged: <commit>
Risk Notes: <text>
Test References: <list>
Build Log IDs: <IDs>
```

---

End of file.
