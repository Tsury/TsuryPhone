# TsuryPhone Home Assistant Integration Implementation Plan

## Overview
This document outlines the implementation plan for adding comprehensive Home Assistant integration to the TsuryPhone project while maintaining support for standalone operation.

## Current Codebase Analysis

### Key Files Examined:
- `main.cpp/h`: Core application logic, state management, dialing logic
- `phoneBook.cpp/h`: Phone number validation and lookup
- `state.cpp/h`: Application state definitions
- `wifi.cpp/h`: WiFi management with optional WebSerial support  
- `config.h`: Hardware and application configuration
- `timeManager.cpp/h`: DND time management
- `ringer.cpp/h`: Ring control logic
- `platformio.ini`: Build environments (debug, debugWebSerial, release)

### Current Features:
- Phone book entries (quick dial)
- DND scheduling
- WiFi configuration portal (port 80)
- Optional WebSerial debug interface (port 32860)
- State machine driven call handling
- Rotary dial input processing

## Implementation Plan

### Phase 1: Foundation & Configuration System ✅ [COMPLETED]

#### 1.1 Update Build Environments ✅ [COMPLETED]
- [x] Update `platformio.ini` with new partition table
- [x] Create base environments to reduce duplication
- [x] Add HOME_ASSISTANT_INTEGRATION environments:
  - `debugHA`
  - `debugWebSerialHA` 
  - `releaseHA`
  - `releaseWebSerialHA`
- [x] Fix ArduinoJson dependency for all environments
- [x] Test compilation of both regular and HA environments

#### 1.2 Create Unified Configuration System ✅ [COMPLETED]
- [x] Design `DeviceConfig` class for unified configuration management
- [x] Create JSON-based persistence on SPIFFS
- [x] Define configuration structure including:
  - Device identification (unique name based on MAC)
  - Quick dial entries
  - Blocked numbers
  - Webhook/remote actions
  - Volume settings (earpiece/speaker, gain/volume)
  - DND settings
  - Ring patterns
- [x] Initialize from current hardcoded values on first run
- [x] Replace direct access to hardcoded values throughout codebase
- [x] Implement immediate persistence for all configuration changes

#### 1.3 Generic Number Handling System ✅ [COMPLETED]
- [x] Create `NumberHandler` class to replace direct phonebook logic
- [x] Support validation against multiple sources:
  - Phone patterns (existing logic)
  - Quick dial entries
  - Webhook actions
  - Blocked numbers
- [x] Ensure mutual exclusivity between quick dial and webhook entries
- [x] Update `main.cpp` to use generic handler
- [x] Integrate blocked number checking with call statistics
- [x] Fix blocked calls logic: blocks incoming calls, not outgoing dialing
- [x] Dynamic phoneBook initialization from generated data instead of hardcoding

### Phase 2: Enhanced Core Features ✅ [COMPLETED]

#### 2.1 Statistics & Monitoring ✅ [COMPLETED]
- [x] Create `DeviceStats` class for tracking:
  - Call counts (total, incoming, outgoing)
  - Blocked call count
  - Total talk time
  - Reset count
  - Uptime tracking
  - Free heap monitoring
- [x] Persistent statistics storage
- [x] Integration with existing state management
- [x] Call start/end tracking in main.cpp

#### 2.2 Enhanced State Management ✅ [COMPLETED]
- [x] Extend `State` structure with additional fields:
  - Current call number
  - Call waiting number
  - Maintenance mode flag
  - Force DND flag
- [x] Real-time state change notifications
- [x] State persistence where appropriate
- [x] Updated TimeManager to use DeviceConfig for DND settings

#### 2.3 Device Identification & Network ✅ [COMPLETED]
- [x] Generate unique device name: `TsuryPhone-<MAC_BASED_ID>`
- [x] Update WiFi SSID to use unique name
- [x] Set call dropped tone duration to 3000ms
- [x] Update config.h with helper functions for device identification

### Phase 3: Home Assistant Integration Infrastructure

#### 3.1 Network Services
- [ ] Create HTTP REST API server (port 8080)
- [ ] Implement WebSocket server for real-time updates
- [ ] Add mDNS service discovery
- [ ] Create API endpoints for all 22 operations
- [ ] Implement JSON request/response handling

#### 3.2 Home Assistant Integration Module
- [ ] Create `HAIntegration` class (conditionally compiled)
- [ ] Implement real-time state synchronization via WebSockets
- [ ] Handle configuration updates from HA
- [ ] Manage maintenance mode with OTA timeout handling
- [ ] Queue and batch updates to minimize network traffic

#### 3.3 Device Operations Implementation
Implement all 22 remote operations:

**Call Control:**
- [ ] Dial custom number
- [ ] Answer incoming call  
- [ ] Hangup ongoing call
- [ ] Switch call waiting

**Configuration:**
- [ ] Force DND on/off
- [ ] Scheduled DND on/off  
- [ ] Maintenance mode toggle
- [ ] Volume/gain settings (4 types)
- [ ] Ring pattern configuration

**Number Management:**
- [ ] Add/remove blocked numbers
- [ ] Add/remove quick dial entries
- [ ] Add/remove webhook actions
- [ ] Ensure mutual exclusivity

**System Control:**
- [ ] Ring operation (custom patterns)
- [ ] Reset device
- [ ] Refetch all data

#### 3.4 State Sensors (27 sensors)
Implement real-time sensor data:

**Call State:**
- [ ] State (idle/ring/incall etc)
- [ ] Call Active, Dialing, Ringing flags
- [ ] Call number, Call waiting number
- [ ] Last call information

**Statistics:**
- [ ] Call counts (blocked, total, incoming, outgoing)
- [ ] Total talk time, Uptime
- [ ] Reset count, Free heap

**Configuration:**
- [ ] DND states, start/end times
- [ ] Device name
- [ ] Quick dial, blocked numbers, webhook entries
- [ ] Volume/gain settings
- [ ] Ring pattern, WiFi RSSI

### Phase 4: Home Assistant Custom Component

#### 4.1 Component Structure
- [ ] Create manifest.json with dependencies
- [ ] Implement config flow for device discovery
- [ ] Add device auto-discovery via mDNS
- [ ] Create device registry entry

#### 4.2 Entity Implementation  
- [ ] Create 27 sensor entities
- [ ] Implement 22 service calls
- [ ] Add proper entity categories and device classes
- [ ] Implement real-time updates via WebSocket

#### 4.3 User Interface
- [ ] Create Lovelace card for device control
- [ ] Add device configuration options
- [ ] Implement service call interfaces
- [ ] Add diagnostic information display

### Phase 5: Integration & Testing

#### 5.1 End-to-End Integration
- [ ] Integrate operations with core phone functionality
- [ ] Test blocked numbers during dialing
- [ ] Verify webhook actions trigger correctly
- [ ] Test volume changes during calls
- [ ] Validate DND behavior
- [ ] Test maintenance mode with OTA

#### 5.2 State Synchronization
- [ ] Ensure all state changes reflect in HA immediately
- [ ] Test device restart state recovery
- [ ] Validate configuration persistence
- [ ] Test network disconnection/reconnection

#### 5.3 Backward Compatibility
- [ ] Verify standalone operation (non-HA environments)
- [ ] Test all existing functionality unchanged
- [ ] Validate build size with new partition table
- [ ] Performance testing with additional features

### Phase 6: Documentation & Finalization

#### 6.1 Documentation
- [ ] API documentation for REST endpoints
- [ ] WebSocket protocol documentation
- [ ] Configuration file format specification
- [ ] Integration setup instructions

#### 6.2 Code Quality
- [ ] Code review and cleanup
- [ ] Remove debug logging where excessive
- [ ] Optimize memory usage
- [ ] Final testing across all environments

## Technical Implementation Details

### File Structure
```
src/
├── integration/
│   └── ha/
│       ├── HAIntegration.h/cpp
│       ├── HAWebServer.h/cpp
│       └── HAWebSocket.h/cpp
├── core/
│   ├── DeviceConfig.h/cpp
│   ├── DeviceStats.h/cpp
│   └── NumberHandler.h/cpp
├── common/ (existing files)
├── components/ (existing files)
└── main.cpp/h (updated)
```

### Preprocessor Usage
- `#ifdef HOME_ASSISTANT_INTEGRATION` used ONLY for:
  - File inclusion in build
  - Wrapping entire files to prevent compilation
  - NOT used for code branching within functions

### Configuration File Format (JSON on SPIFFS)
```json
{
  "device": {
    "name": "TsuryPhone-A1B2C3",
    "resetCount": 5
  },
  "audio": {
    "earpieceVolume": 2,
    "earpieceGain": 7,
    "speakerVolume": 7,
    "speakerGain": 7
  },
  "dnd": {
    "force": false,
    "scheduled": true,
    "startHour": 18,
    "startMinute": 30,
    "endHour": 8,
    "endMinute": 30
  },
  "quickDial": {
    "211": "0546662771",
    "212": "0524618858"
  },
  "blockedNumbers": [
    "0501234567"
  ],
  "webhookActions": {
    "3452": "gfno=0tdop4gkotrpo"
  },
  "ringPattern": "500,500,500,500x3"
}
```

## Build Commands
Test compilation: `&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment ENV`

## Current Status: Phase 3 - Home Assistant Integration Infrastructure [READY TO START]

### Completed:
- ✅ Phase 1: Foundation & Configuration System - Complete
- ✅ Phase 2: Enhanced Core Features - Complete
- ✅ Build system updated with all environments
- ✅ Unified configuration management implemented
- ✅ Generic number handling system created
- ✅ Statistics tracking integrated
- ✅ Device identification and unique naming implemented
- ✅ All code compiles successfully for both regular and HA environments

### Next Steps:
1. Begin Phase 3: Home Assistant Integration Infrastructure
2. Create HTTP REST API server (port 8080)
3. Implement WebSocket server for real-time updates  
4. Add mDNS service discovery
5. Create HAIntegration class (conditionally compiled)

## Questions for Clarification

Before proceeding with implementation, please confirm:

1. **Partition Table**: The provided partition table allocates 192KB each for app0/app1 OTA slots. Is this sufficient for the expanded codebase?

2. **Network Protocol**: For real-time updates, should we use WebSockets exclusively, or complement with Server-Sent Events for better browser compatibility?

3. **Configuration Persistence**: Should device configuration changes via HA be immediately persistent, or batched/debounced to reduce SPIFFS wear?

4. **Error Handling**: What level of error handling is desired for network operations (connection failures, invalid requests, etc.)?

5. **Memory Management**: Any specific memory constraints or monitoring requirements beyond the free heap sensor?

The plan is comprehensive and follows all specified guidelines. Ready to proceed with implementation upon confirmation of any clarifications needed.
