# TsuryPhone ↔ Home Assistant Integration Communication Map

Status: Generated reference (auto-derived from source as of current commit)

Scope: TsuryPhone firmware project (`TsuryPhone/`) – Home Assistant (HA) integration only. Android or other future integrations are excluded. This document enumerates EVERY current external communication surface (HTTP REST, WebSocket, outbound webhooks) plus internal command → handler → side‑effect chains and emitted event payload shapes. Source files analyzed:

* `src/integration/ha/HAWebServer.cpp`
* `src/integration/ha/HAIntegration.cpp`
* `src/integration/IntegrationService.cpp`
* `src/integration/IntegrationManager.cpp`

---

## 1. Inbound Interfaces (HA → Device)

### 1.1 HTTP Endpoints
All endpoints served by `HAWebServer` on TCP port `kServerPort` (see header; defaults typically 80/8080). CORS enabled (`*`). Unless noted: Method = POST for state‑changing operations, GET for fetch operations. JSON responses use `Content-Type: application/json`.

Successful command responses share structure:
```
{ "success": true, "data": <optional JsonVariant> }
```
Error responses:
```
{ "success": false, "message": "<error text>" }
```
HTTP status code: Not Found → 404; others typically default (implementation header likely sets 400; unspecified calls rely on default parameter in declaration).

| Path | Method | Command Routed | Body Schema (required / optional) | Validation & Error Conditions | Downstream Handler Chain | Side Effects | Response Data (when success) |
|------|--------|----------------|-----------------------------------|-------------------------------|--------------------------|-------------|------------------------------|
| /api/config/tsuryphone | GET | tsuryphone_config | none | n/a | `HAIntegration::handleGetTsuryPhoneConfig` → `IntegrationService::buildDeviceConfig` (+ HA webhook decorations) | None (read only) | `{deviceId, phone.webhooks[], phone.homeAssistantUrl}` (plus any future fields) |
| /api/refetch_all | GET | refetch_all | none | n/a | `HAIntegration::handleRefetchAll` → `IntegrationService::handleRefetchAll()` (reloads config & stats) | Reloads persisted config & stats from SPIFFS | On success `data` replaced with full snapshot (`IntegrationService::buildAllData()`) |
| /api/diagnostics | GET | (special, direct) | none | n/a | `HAWebServer::handleDiagnostics` → `populateCoreDiagnostics` | None | Diagnostics object (capabilities, metrics, etc.) |
| /api/call/dial | POST | dial | `{ number: string }` | Missing `number` → 400 / error | `IntegrationService::handleDialRequest` → dialCallback (PhoneApp) | Initiates call | No extra data unless callback populates `IntegrationCallbackResult.data` |
| /api/call/dial_quick_dial | POST | dial_quick_dial | `{ code: string }` | Missing `code` | `IntegrationService::handleDialQuickDial` → `handleDialRequest` | Dials resolved number | Same as dial |
| /api/call/answer | POST | answer | none | n/a | `IntegrationService::handleAnswerRequest` → answerCallback | Answers ringing call | No data |
| /api/call/hangup | POST | hangup | none | n/a | `IntegrationService::handleHangupRequest` → hangupCallback | Terminates active call | No data |
| /api/call/switch_call_waiting | POST | switch_call_waiting | none | n/a | `IntegrationService::handleToggleCallWaiting` → callWaitingCallback | Toggles call waiting | No data |
| /api/system/ring | POST | ring | `{ pattern?: string (non-empty if present) }` | Provided empty string → error | `IntegrationService::handleRingOperation` → ringCallback | Plays ring (pattern or default) | No data |
| /api/system/reset | POST | reset | none | n/a | `IntegrationService::handleResetDevice` (schedules reboot) | Schedules device restart in ~2.5s (plus 500ms delay before ESP.restart) | No data |
| /api/config/dnd | POST | dnd | `{ enabled: bool, force?: bool, scheduled?: bool, startHour?: int, startMinute?: int, endHour?: int, endMinute?: int }` | Missing or non-bool `enabled` rejected by web layer; optional fields individually type-checked in service | `IntegrationService::handleSetDND` (mutates config if any param changed) | Potential config write; updates DND active state indirectly | `{ dndActive: bool }` |
| /api/config/maintenance | POST | maintenance_mode | `{ enabled: bool }` | Missing/invalid `enabled` | `IntegrationService::handleSetMaintenanceMode` (updates `_state.isMaintenanceMode`) | May invoke maintenance mode callback | `{ isMaintenanceMode: bool }` |
| /api/config/audio | POST | audio_config | `{ earpieceVolume?, earpieceGain?, speakerVolume?, speakerGain? }` (ints 1–7) | No JSON object; or none of the four fields; or any value out of [1,7] range → specific error | `IntegrationService::handleSetAudioConfig` | Writes audio config if valid | `{ config: { audio: { earpieceVolume, earpieceGain, speakerVolume, speakerGain }}}` (post-update snapshot) |
| /api/config/ring_pattern | POST | ring_pattern | `{ pattern: string }` | Missing `pattern` or empty string → error | `IntegrationService::handleSetRingPattern` | Stores pattern in config | `{ ringPattern: string }` |
| /api/config/quick_dial_add | POST | quick_dial_add | `{ code: string, number: string, name?: string }` | Missing JSON object; missing `code` or `number`; duplicate code; internal add failure | `IntegrationService::handleAddQuickDial` | Adds quick dial entry | `{ entry: { code, number, name }}` |
| /api/config/quick_dial_remove | POST | quick_dial_remove | `{ code: string }` | Missing object / missing `code` | `IntegrationService::handleRemoveQuickDial` | Removes quick dial entry | No data |
| /api/config/blocked_number_add | POST | blocked_number_add | `{ number: string, reason?: string }` | Missing object / missing `number`; empty number | `IntegrationService::handleAddBlockedNumber` | Adds blocked number | `{ entry: { number, reason }}` |
| /api/config/blocked_number_remove | POST | blocked_number_remove | `{ number: string }` | Missing object / missing `number` | `IntegrationService::handleRemoveBlockedNumber` | Removes blocked number | No data |
| /api/config/webhook_add | POST | webhook_add | `{ code: string, id: string, actionName?: string }` | Missing object / missing `code` or `id`; empty; code conflict with quick dial or existing webhook | `HAIntegration::handleAddWebhookAction` (`_haConfig.addWebhookAction`) | Adds outbound webhook action mapping | `{ entry: { code, id, actionName }}` |
| /api/config/webhook_remove | POST | webhook_remove | `{ code: string }` | Missing object / missing `code`; code not found | `HAIntegration::handleRemoveWebhookAction` | Removes webhook mapping | No data |
| /api/config/ha_url | POST | ha_url | `{ url: string }` | Missing object / missing `url`; empty URL | `HAIntegration::handleSetHAUrl` | Stores new HA base URL | No data |

### 1.2 WebSocket (`/ws`)
Transport: Text frames with JSON.

Inbound (client → device) message schema:
```
{ "command": <string>, "data": <JsonVariant optional> }
```
Allowed `command` values mirror HTTP command column above plus: `refresh` (forces full state broadcast). Validation and processing path identical to HTTP (`HAIntegration::setStateUpdateCallback` switch). No direct per-message response is currently sent over the WebSocket (silent processing).

Connection lifecycle events logged only (connect / disconnect). Ping/Pong delegated to AsyncWebSocket internals.

### 1.3 Command Dispatch Chain
Example (dial):
`POST /api/call/dial` → `HAWebServer::handleDialNumber` (validates number) → `executeCommand("dial")` → callback bound in `HAIntegration::setupWebServerCallbacks` → `HAIntegration::handleDialRequest` → `IntegrationService::handleDialRequest` → `_dialCallback` (PhoneApp) → result bubbles back, converted to `HAOperationResult` → HTTP JSON response.

All commands follow the same segments: WebServer Handler (validation) → `executeCommand` → HAIntegration switch → IntegrationService handler (generic) → optional PhoneApp primitive callback.

### 1.4 Validation Summary (Service Layer)
| Operation | Additional Generic Validation (beyond Web layer) |
|-----------|-----------------------------------------------|
| Dial | Non-empty number; callback existence |
| Answer | Callback existence |
| Hangup | Callback existence |
| Quick Dial Dial | Code exists; resolves to number then reuses Dial validation |
| Toggle Call Waiting | Callback existence |
| Ring Operation | Callback existence |
| DND Set | Each provided field type-checked; only fields present mutated |
| Maintenance Mode | None (just sets state + callback) |
| Audio Config | Each provided param in [1,7] range else error; at least one param required (web layer) |
| Ring Pattern | Not empty |
| Quick Dial Add | Code & number non-empty; code must not already exist |
| Quick Dial Remove | Code non-empty; must exist (failure otherwise) |
| Blocked Number Add | Number non-empty; must not already exist (failure message generic) |
| Blocked Number Remove | Number non-empty; must exist |
| Reset Device | Schedules deferred reset (2.5s) |
| Refetch All | Reloads config + stats files |

### 1.5 Error Message Inventory
Representative (exact string values):
```
"Invalid JSON"
"Missing 'number' parameter"
"Missing 'code' parameter"
"Missing or invalid 'enabled' parameter (must be boolean)"
"Parameter 'pattern' cannot be empty string if provided"
"Invalid JSON object"
"At least one audio parameter must be provided: earpieceVolume, earpieceGain, speakerVolume, or speakerGain"
"Missing required parameters: 'code' and 'number'"
"Missing required parameter: 'code'"
"Missing required parameters: 'code' and 'id'"
"Missing required parameter: 'number'"
"Missing required parameter: 'pattern'"
"Missing required parameter: 'url'"
"Dial callback not available"
"Answer callback not available"
"Hangup callback not available"
"Ring callback not available"
"Call waiting callback not available"
"Number cannot be empty"
"Code and number cannot be empty"
"Code already exists in quick dial entries"
"Failed to add quick dial entry"
"Failed to remove quick dial entry or entry not found"
"Quick dial code not found"
"Missing 'code' parameter"
"Ring pattern cannot be empty"
"Failed to add blocked number (may already exist)"
"Blocked number not found"
"Code and webhook ID cannot be empty"
"Code already exists in quick dial or webhook actions"
"Failed to add webhook action"
"Webhook action not found"
"URL cannot be empty"
"Earpiece volume must be between 1 and 7" (and analogous for other gains/volumes)
"Service not available"
"Unknown command: <cmd>"
```

---

## 2. Outbound Interfaces (Device → HA / Clients)

### 2.1 WebSocket Broadcast Events
All outbound events originate from `HAIntegration` using builders in `IntegrationService`. Transport: WS text JSON. Dispatch triggers include state changes detected by `IntegrationManager::checkForStateChanges`, explicit operation success handlers, periodic timers (stats/system), and full-state broadcasts.

#### 2.1.1 Event Object Field Semantics (CURRENT IMPLEMENTATION)
Builder `createEventObject(doc, event, type)` outputs root fields:
```
{
  schemaVersion: <int>,        // INTEGRATION_EVENT_SCHEMA_VERSION
  event: <category>,           // e.g. "call", "phone_state", "system", "full_state"
  type: <subType>,             // e.g. "start", "end", "state", "stats", "shutdown"
  ts: <uint ms since boot>,
  deviceId: <string>
  ...additional per builder...
}
```
NOTE: This ordering (event=category, type=subType) differs from design doc draft that inversed naming.

#### 2.1.2 Builders & Additional Fields
| Builder | Category (`event`) | Subtype (`type`) inputs | Added Fields | Trigger Points |
|---------|--------------------|-------------------------|--------------|---------------|
| buildCallEvent(eventType, number, isIncoming, duration) | "call" | `start` / `end` / `blocked` | `number` (if non-empty); `isIncoming` (start only); `duration` (end if >0) | Call start/end/block events from `HAIntegration::report*` |
| buildPhoneStateEvent(eventType, newState, prevState, currentNumber) | "phone_state" | `state` / `call_info` / `dialing` / `ring` / `dnd` | For `state`: `state`, `previousState`, `stateName`, plus phone state info (dndActive, maintenanceMode, currentCallNumber, currentDialingNumber). For others: `currentCallNumber` (call_info), `currentDialingNumber` (dialing), `isRinging`=true (ring), `dndActive` (dnd). | State changes / dialing progress / ring toggle / DND changes |
| buildCurrentPhoneStateEvent(eventType) | (delegates) | same as above | Same as above | HAIntegration convenience wrappers |
| buildSystemEvent(eventType) | "system" | `stats` / `status` | `calls,incoming,outgoing,blocked,talkTime` (stats) OR system metrics (`freeHeap`,`rssi`,`uptime`) | Periodic timers (stats/system) |
| buildShutdownEvent(reason) | "system" | `shutdown` | `reason`, `message` | Before scheduled reset (`handleResetDevice`) |
| buildErrorEvent(error) | "system" | `error` | `error` | `reportError` path |
| buildFullStateEvent(callNumber,isIncoming,startTime) | "full_state" | (empty string) | Aggregated phone/system/call/stats snapshot | Full state broadcast (init, config changes) |
| buildCurrentFullStateEvent() | "full_state" | "" | Delegates with current call info | `broadcastFullState()` |
| buildCurrentCallEvent(eventType,duration) | "call" | `start`/`end`/`blocked` | Similar to buildCallEvent but uses internal state (note: `isIncoming` currently default false) | (Not used directly by HAIntegration after refactor) |
| buildSystemConfigEvent(key,value) | "system" | `config` | dynamic key inserted (`obj[key]=value`) | `broadcastStateChange` (HAIntegration) |

#### 2.1.3 Phone State Info Helpers
`addPhoneStateInfo` sets: `state` (enum int), `stateName`, `dndActive`, `isMaintenanceMode`, `currentCallNumber` (if active), `currentDialingNumber` (if dialing). `addCallInfo` may also append `currentCallIsIncoming`, `currentCallStartTime`, and `currentCallDuration` (derived if `startTime > 0`).

### 2.2 Timing / Periodic Broadcasts
* Stats every `kStatsUpdateInterval` ms (see HAIntegration constant) → `system` / `stats` event.
* System status every `kSystemUpdateInterval` ms → `system` / `status` event.
* Full state on integration init and on configuration change triggers.

### 2.3 Outbound Webhook Invocations (Device → HA Core)
Triggered via dialing action codes mapped to webhook actions or integration manager `triggerAction` flows:
* Function: `HAIntegration::triggerAction(actionId)`
* Preconditions: WiFi must be `WL_CONNECTED` else skipped.
* URL: `<homeAssistantUrl>/api/webhook/<actionId>` (method POST, empty body, timeout 5s)
* Success: Logs HTTP code (>=0). No retry logic.
* Failure: Logs error string from `HTTPClient`.
* Registration: Added through `/api/config/webhook_add` (code ↔ webhook id). Removal via `/api/config/webhook_remove`.

### 2.4 Action Code Resolution (Dial Pad → Webhook / Quick Dial / Future)
Managed by `IntegrationManager` with registered `IIntegrationActionHandler` implementations (HA supplies `_haNumberHandler`). Dialed partial codes can be detected (`hasPartialActionMatch`) and full matches resolved to action IDs (`resolveActionId`). After resolution, `triggerAction` can dispatch (subject to capability flag `IC_ACTIONS`).

---

## 3. Internal Event Detection & Propagation Flow
Source: `IntegrationManager::checkForStateChanges()` handles transitions:
1. App state change → updates stats, detects call start/end transitions, invokes `updatePhoneState` and possibly `reportCallStart` / `reportCallEnd`.
2. Call number change → updates stats, may block call (`shouldBlockCall`) leading to `reportBlockedCall` else `updateCallInfo`.
3. Dialing number change → `updateDialingProgress`.
4. DND state change → `updateDndState`.
5. Maintenance mode change → triggers `updatePhoneState` again.
6. Ring state change (incoming ring entry/exit) → `updateRingState`.

Each `update*` method loops integrations; HAIntegration builds and broadcasts corresponding events via `IntegrationService`.

---

## 4. Configuration Change Events (Internal)
Enum `ConfigChangeEvent` (names inferred from mapping in `notifyConfigChange`):
* `DND_CONFIG_CHANGED`
* `AUDIO_CONFIG_CHANGED`
* `QUICK_DIAL_CHANGED`
* `BLOCKED_NUMBER_CHANGED`
* `RING_PATTERN_CHANGED`
* `INTEGRATION_EXTENSION_CHANGED`

Generic service handlers mutate config but do not themselves call `notifyConfigChange`; higher-level code is expected (comment markers `// Config change event emitted externally (C2)`). HA currently responds by re‑broadcasting full state on configuration changes via `onConfigurationChanged` (trigger path external to the shown code).

---

## 5. Reset Workflow
1. `/api/system/reset` or WebSocket `{command:"reset"}` accepted.
2. `HAIntegration::handleResetDevice` broadcasts immediate `system/shutdown` event with reason `reset_requested`.
3. `IntegrationService::handleResetDevice` schedules flag `_resetRequested` for ~2.5s later.
4. `HAIntegration::process()` invokes `IntegrationService::processScheduledReset()`, which after delay logs and calls `ESP.restart()`.

---

## 6. Security / Auth Considerations (Current State)
* No authentication / authorization; all endpoints open (CORS `*`).
* Webhook outbound requires previously configured HA base URL; no token usage.
* mDNS advertises service with TXT keys `device` and `version`.

---

## 7. Known Divergences vs Design Plan
| Design Plan Expectation | Actual Implementation | Note |
|-------------------------|-----------------------|------|
| Root event fields: `type`=category, `event`=subtype | `event`=category, `type`=subtype | Potential future rename for alignment |
| Call event end field name `durationMs` | Uses `duration` (seconds for ended call), and optional `currentCallDuration` in snapshot helpers | Harmonization advisable |
| Include `isIncoming` on all call events | Present only on `start`; `blocked` sets `isIncoming` indirectly (in builder usage sets true) but not enforced | Consider consistency |
| Full state emission field harmonization | Mixed usage of `addPhoneStateInfo` & `addCallInfo` produces `currentCallNumber` plus `currentCallIsIncoming` / `currentCallStartTime` only when startTime>0 | Clarify spec |

---

## 8. Summary Tables

### 8.1 Command → Service Handler Matrix
| Command | Service Method | Callback Dependency | Config Mutation | State Mutation | Event Emitted (Immediate) |
|---------|----------------|--------------------|-----------------|----------------|---------------------------|
| dial | handleDialRequest | _dialCallback | No | No (PhoneApp triggers later state changes) | Potential call start later |
| answer | handleAnswerRequest | _answerCallback | No | No | Phone state event (after state change) |
| hangup | handleHangupRequest | _hangupCallback | No | No | Call end + state events (after state change) |
| dial_quick_dial | handleDialQuickDial | _dialCallback | No | No | As per dialing path |
| switch_call_waiting | handleToggleCallWaiting | _callWaitingCallback | Possibly external system | No | None immediate |
| ring | handleRingOperation | _ringCallback | No | No | Ring state event (if state change) |
| dnd | handleSetDND | (none) | Yes (DND config) | Indirect (affects `dndActive`) | DND phone_state event |
| maintenance_mode | handleSetMaintenanceMode | _maintenanceModeChangedCallback optional | No | Yes (`isMaintenanceMode`) | Phone_state state event (via update) |
| audio_config | handleSetAudioConfig | (none) | Yes (Audio) | No | None (full state may be broadcast externally) |
| ring_pattern | handleSetRingPattern | (none) | Yes (Ring pattern) | No | None (full state possible) |
| quick_dial_add | handleAddQuickDial | (none) | Yes | No | None (full state possible) |
| quick_dial_remove | handleRemoveQuickDial | (none) | Yes | No | None |
| blocked_number_add | handleAddBlockedNumber | (none) | Yes | No | None |
| blocked_number_remove | handleRemoveBlockedNumber | (none) | Yes | No | None |
| webhook_add | handleAddWebhookAction | (none) | HA config extension | No | Full state broadcast |
| webhook_remove | handleRemoveWebhookAction | (none) | HA config extension | No | Full state broadcast |
| ha_url | handleSetHAUrl | (none) | HA URL config | No | Full state broadcast |
| tsuryphone_config | handleGetTsuryPhoneConfig | (none) | No | No | None (response only) |
| refetch_all | handleRefetchAll | (none) | Reload from storage | No | None (response supplies snapshot) |
| reset | handleResetDevice | (none) | No | Schedule restart | Immediate shutdown event, later reboot |
| refresh (WS only) | broadcastFullState | (none) | No | No | Full state event |

---

## 9. Suggested Normalization Opportunities (Non-Blocking)
* Align root event field semantics with design spec (swap or rename `event`/`type`).
* Standardize call duration field naming (`duration` vs `durationMs` vs `currentCallDuration`).
* Add `isIncoming` to all call-related events for consistency (end, blocked).
* Consider including an incrementing sequence id in all emitted event payloads (currently only structured logs have a `seq`).
* Introduce optional ACK / response pattern for WebSocket commands.

---

## 10. Glossary
| Term | Meaning |
|------|---------|
| Quick Dial | User-defined mapping code → phone number. |
| Webhook Action | Code → HA webhook id used for outbound POST trigger. |
| Action Handler | Abstraction resolving dialed input to action IDs (webhooks, etc.). |
| Full State Broadcast | Aggregated snapshot built by `buildCurrentFullStateEvent`. |

---

End of document.
