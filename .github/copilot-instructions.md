# TsuryPhone ESP32 Firmware - AI Coding Agent Instructions

## Project Overview

This repository contains the **ESP32 firmware** for TsuryPhone - a vintage rotary phone converted to work with modern cellular networks via ESP32 hardware. The firmware can optionally integrate with Home Assistant.

**Related Repository:** [`ha-tsuryphone`](https://github.com/Tsury/ha-tsuryphone) - Home Assistant custom integration

## Core Architecture

### Build System
PlatformIO with multiple environments for different feature combinations:
```bash
# Development builds
pio run -e debug                # Basic phone functionality only
pio run -e debugWebSerial       # + Web serial debugging at :32860
pio run -e debugHA              # + Home Assistant integration
pio run -e debugWebSerialHA     # + Both WebSerial and HA features

# Production builds (optimized)
pio run -e release              # Basic functionality
pio run -e releaseHA            # + Home Assistant integration
pio run -e releaseWebSerialHA   # All features enabled
```

### Key Components
- `src/main.cpp` - Core phone application with state machine
- `src/common/homeAssistantServer.cpp` - Optional HTTP/WebSocket server 
- `src/components/` - Hardware abstraction layer:
  - `modem.cpp` - GSM/cellular communication (TinyGSM)
  - `ringer.cpp` - Bell/ringer control
  - `rotaryDial.cpp` - Rotary dial input processing
  - `hookSwitch.cpp` - On/off hook detection
- `src/common/wifi.cpp` - WiFiManager for network configuration
- `generated/` - Auto-generated phonebook and MP3 data

### Critical Pattern: Optional Home Assistant Integration
```cpp
#ifdef HOME_ASSISTANT_INTEGRATION
// HA-specific code - completely optional, zero impact when disabled
#include "common/homeAssistantServer.h"
HomeAssistantServer haServer;
#endif
```

## Development Workflows

### Standard Development Cycle
```bash
# Build and flash firmware
pio run -e debugHA -t upload    # HA-enabled build
pio device monitor              # View serial output

# For WebSerial debugging (when enabled)
# Access http://device-ip:32860/webserial in browser
```

### Code Generation (Critical)
When adding contacts or audio files, regenerate headers:
```bash
cd phoneBook && python generate.py  # Generates src/generated/phoneBook.h
cd mp3 && python generate.py        # Generates src/generated/mp3.h and mp3.cpp
```

### Hardware Dependencies
- **Board:** ESP32-WROVER with PSRAM (LilyGO T-Call A7670 V1.0)
- **Modem:** A7670 GSM module via TinyGSM library
- **Storage:** SPIFFS for configuration (when HA integration enabled)

## Key Patterns & Conventions

### State Machine Architecture
```cpp
// Central state management
void PhoneApp::setState(const AppState newState) {
    if (_state.newAppState != newState) {
        _state.prevAppState = _state.newAppState;
        _state.newAppState = newState;
        onStateChanged();  // Triggers state-specific logic
    }
}

// State processing in main loop
void PhoneApp::loop() {
    processState();  // Handle current state logic
    // Update all components
    _modem.process();
    _ringer.process();
    _hookSwitch.process();
    _rotaryDial.process();
    _wifi.process();
#ifdef HOME_ASSISTANT_INTEGRATION
    haServer.process(_state);
#endif
}
```

### Global HA Callback Pattern
```cpp
#ifdef HOME_ASSISTANT_INTEGRATION
// Global callbacks for HA server to invoke phone actions
PhoneApp *g_phoneApp = nullptr;  // Global instance pointer

void haPerformCall(const char *number) {
    if (g_phoneApp) {
        g_phoneApp->haPerformCall(number);
    }
}
#endif
```

### Memory Management
```cpp
// Use F() macro for static strings to save RAM
Logger::infoln(F("Phone state changed to: %s"), stateString);

// Limit JSON buffer sizes for ESP32 constraints
const constexpr int kJsonBufferSize = 1024;
```

### Hardware Abstraction
Each component follows standard init/process pattern:
```cpp
class Component {
public:
    void init();     // Setup hardware
    void process();  // Main loop processing
    // Component-specific methods
};
```

## Home Assistant Integration (Optional)

### HTTP Server (when enabled)
- **Port 80:** REST API with unified `/action` endpoint
- **mDNS:** `tsuryphone.local` discovery
- **WebSocket:** `/ws` for real-time state updates
- **SPIFFS:** JSON configuration storage

### API Design
```cpp
// Unified action endpoint handles all HA requests
POST /action
{
  "action": "call_custom",
  "number": "1234567890"
}

// State broadcasting via WebSocket
GET /ws  // Real-time state updates
```

### Configuration Storage
```cpp
// SPIFFS-based JSON configuration for HA settings
struct HaConfig {
    bool dndForceEnabled;
    std::vector<std::pair<String, String>> phoneBook;
    std::vector<String> blockedNumbers;
    String haServerUrl;  // For webhook callbacks
};
```

## Critical Build Dependencies

### Always Required
```ini
[env]
lib_deps = 
    https://github.com/lewisxhe/TinyGSM-fork.git
    tzapu/WiFiManager@^2.0.17
```

### HA Integration Builds Only
```ini
[env:debugHA]
build_flags = -DHOME_ASSISTANT_INTEGRATION
lib_deps =
    ${env.lib_deps}
    ESP32Async/ESPAsyncWebServer@^3.7.9
    bblanchon/ArduinoJson@^7.4.2
```

## Debugging & Testing

### WebSerial Access (when enabled)
- **URL:** `http://device-ip:32860/webserial`
- **Features:** Bidirectional serial communication via web browser
- **Memory monitoring:** Free heap displayed every 60s

### Direct API Testing
```bash
# Test device health
curl http://device-ip/status
curl http://device-ip/stats

# Trigger actions (when HA enabled)
curl -X POST http://device-ip/action \
  -H "Content-Type: application/json" \
  -d '{"action":"call_custom","number":"1234567890"}'
```

### Common Issues
- **Memory exhaustion:** Monitor free heap, use F() macros
- **Network timeouts:** WiFiManager portal timeout is 5 minutes
- **Build failures:** Ensure correct environment selected
- **Integration mismatch:** HA integration requires `HOME_ASSISTANT_INTEGRATION` flag

## Code Organization Principles

### Conditional Compilation
Always use `#ifdef` guards for optional features:
```cpp
#ifdef HOME_ASSISTANT_INTEGRATION
// HA-specific code
#endif

#ifdef WEB_SERIAL  
// WebSerial debug code
#endif
```

### Error Handling
```cpp
// Validate all inputs, especially from network
if (!data || len == 0) {
    sendError(request, "Missing data");
    return;
}
```

### Logging Strategy
```cpp
// Use appropriate log levels
Logger::infoln(F("Important state changes"));
Logger::errorln(F("Error conditions"));
Logger::debugln(F("Verbose debugging"));  // Only in debug builds
```

When making changes, always test both HA-enabled and basic builds to ensure the integration remains truly optional and doesn't break core phone functionality.
