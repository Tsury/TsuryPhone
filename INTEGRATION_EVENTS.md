# TsuryPhone Integration Event Contract (schemaVersion 2)

Forward-only v2 schema (no legacy dual emission). Root fields:
`schemaVersion`, `seq`, `ts`, `integration`, `deviceId`, `category`, `event`.

## 1. Categories & Subtypes
| category | subtypes | notes |
|----------|----------|-------|
| call | start, end, blocked, missed | Call lifecycle (durationMs only on end) |
| phone_state | state, dialing, ring, dnd, call_info | Real-time phone UI/state facets |
| system | status, stats, shutdown, error | Device/system health & control |
| config | config_delta | Configuration mutation signaling |
| diagnostic | (future) | Reserved for verbose diagnostics |

## 2. Base Envelope
```
{
  schemaVersion: 2,
  seq: <uint32>,
  ts: <uint64 ms>,
  integration: "ha" | <other>,
  deviceId: <string>,
  category: <string>,
  event: <string>
}
```

## 3. Event Payloads
### 3.1 Call
Common: `number`, `isIncoming`, `callStartTs` (except blocked may omit), plus subtype specifics.
| subtype | required | optional |
|---------|----------|----------|
| start | number,isIncoming,callStartTs |  |
| end | number,isIncoming,callStartTs,durationMs |  |
| blocked | number,isIncoming | callStartTs,blockedReason |
| missed | number,isIncoming | callStartTs |

### 3.2 Phone State
Envelope + `state`,`stateName`,`isMaintenanceMode`. Optional: `previousState`,`currentCallNumber`,`isIncomingCall`,`currentDialingNumber`,`isRinging`,`dndActive`.

### 3.3 System
| subtype | fields |
|---------|--------|
| status | uptimeMs,freeHeap,rssi |
| stats | calls { total,incoming,outgoing,blocked,talkTimeSeconds } |
| shutdown | reason,message |
| error | errorCode,errorMessage |

### 3.4 Config Delta
Two forms:
Single change:
```
{
  ...base,
  category: "config",
  event: "config_delta",
  key: "ring.pattern",
  oldValue: "triple_fast",   // optional
  newValue: "double_slow"
}
```
Aggregated (audio / DND batches):
```
{
  ...base,
  category: "config",
  event: "config_delta",
  changes: [
    { key: "audio.earpieceVolume", oldValue: 3, newValue: 5 },
    { key: "audio.speakerGain", oldValue: 2, newValue: 4 }
  ]
}
```

### 3.5 Success / Error Responses (HTTP)
Success:
```
{ schemaVersion:2, success:true, ts:<ms>, data?:{...} }
```
Error:
```
{ schemaVersion:2, success:false, ts:<ms>, errorCode:<string>, message:<string> }
```

## 4. Naming Rules
CamelCase only. Millisecond fields suffixed `Ms` (durationMs, uptimeMs). Booleans with `is*` prefix when semantic (isIncoming, isMaintenanceMode).

## 5. Config Key Namespace (Dotted)
`audio.*`, `dnd.*`, `ring.pattern`, `maintenance.enabled`, `quick_dial.add/remove`, `blocked_number.add/remove`, `webhook.add/remove`, `ha.url`.

## 6. Guarantees
1. Monotonic `seq` (wrap on 32-bit overflow).
2. `schemaVersion` frozen at 2 until next migration.
3. No deprecated v1 fields (`type`, second-based durations) appear.
4. Aggregated config deltas never mix with single-form fields in same object.

## 7. Extension Guidance
Additive only: introduce new optional fields or new `category` / `event` values; never repurpose existing names without version bump.

## 8. Examples
Call end:
```
{ schemaVersion:2, seq:91, ts:1736262455000, integration:"ha", deviceId:"ABC123", category:"call", event:"end", number:"+15551234567", isIncoming:true, callStartTs:1736262449000, durationMs:6000 }
```
Audio batch delta:
```
{ schemaVersion:2, seq:94, ts:1736262459000, integration:"ha", deviceId:"ABC123", category:"config", event:"config_delta", changes:[ {key:"audio.earpieceVolume",oldValue:3,newValue:5} ] }
```

## 9. Future
Diagnostic category for verbose metrics; optional schema hash field for integrity.

---
Document status: v2 canonical. Update only with additive changes or a version bump proposal.
