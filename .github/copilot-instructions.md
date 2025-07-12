# TsuryPhone ESP32 Firmware - AI Coding Agent Instructions

## Project Overview

This repository contains the **ESP32 firmware** for TsuryPhone - a vintage rotary phone converted to work with modern cellular networks via ESP32 hardware. The firmware integrates with Home Assistant and supports multiple server architectures.

**Language:** C++11 for ESP32 platform  
**Related Repository:** [`ha-tsuryphone`](https://github.com/Tsury/ha-tsuryphone) - Home Assistant custom integration

## Core Architecture

### Build System
PlatformIO with multiple environments for different feature combinations:

**Windows Build Commands:**
```powershell
# Development builds  
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment debug                # Basic phone functionality
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment debugWebSerial       # + Web serial debugging at :32860
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment debugHA              # + Home Assistant integration
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment debugWebSerialHA     # + Both WebSerial and HA features

# Production builds (optimized)
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment release              # Basic functionality
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment releaseHA            # + Home Assistant integration
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment releaseWebSerialHA   # All features enabled
```

### Modern Folder Architecture

```
src/
├── core/              # 📱 Core phone application logic
│   ├── main.cpp       # Main phone application (PhoneApp class)
│   ├── main.h
│   ├── phoneController.h  # Phone control interface
│   ├── phoneValidation.cpp/h  # Phone number validation
│   ├── state.cpp      # Phone state management
│   └── state.h
├── hardware/          # 🔧 Physical device interfaces
│   ├── hookSwitch.cpp/h   # Phone hook switch
│   ├── modem.cpp/h        # GSM/Modem interface (TinyGSM)
│   ├── ringer.cpp/h       # Bell/ringer control
│   └── rotaryDial.cpp/h   # Rotary dial input processing
├── config/            # ⚙️ Configuration and data management
│   ├── corePhoneConfig.cpp/h      # Centralized phone config (DnD, quick dial, blocked numbers, phonebook, maintenance mode)
│   └── configProvider.cpp/h      # Legacy config provider (deprecated, use CorePhoneConfig)
├── network/           # 🌐 Network and connectivity
│   ├── wifi.cpp/h         # WiFiManager for network configuration
│   └── timeManager.cpp/h  # Time synchronization
├── servers/           # 🖥️ Multi-server architecture
│   ├── server.h              # Server interface
│   ├── serverManager.cpp/h  # Server coordination
│   ├── serverFactory.cpp/h  # Server creation factory
│   └── homeAssistantServer.cpp/h  # HA integration server
├── utils/             # 🛠️ Cross-cutting utilities
│   ├── logger.cpp/h     # Logging system
│   ├── string.cpp/h     # String utilities
│   ├── stream.cpp/h     # Stream helpers
│   ├── ringBuffer.h     # Ring buffer template
│   └── consts.h         # Constants
├── generated/         # 🤖 Auto-generated files
└── config.h + entry.cpp (root level)
```

### Multi-Server Architecture

The firmware supports multiple server types through a unified server management system:
- **CorePhoneConfig**: Centralized shared storage for quick dial entries, blocked numbers, DnD settings, phonebook entries, and maintenance mode
- **ServerManager**: Coordinates all servers and aggregates webhooks across servers
- **ServerFactory**: Auto-registers available servers at runtime
- **IServer Interface**: Each server implements webhook methods (`hasWebhookEntry`, `executeWebhook`, etc.)
- **HomeAssistantServer**: HTTP/WebSocket server for HA integration (when enabled)

### Key Components
- `core/main.cpp` - Core phone application with state machine (PhoneApp class)
- `servers/homeAssistantServer.cpp` - HTTP/WebSocket server for HA integration
- `hardware/` - Hardware abstraction layer for all physical components
- `config/corePhoneConfig.cpp` - Centralized configuration with SPIFFS persistence
- `network/wifi.cpp` - WiFiManager for network configuration
- `generated/` - Auto-generated phonebook and MP3 data

### Automatic Local Config Seeding

`CorePhoneConfig` automatically seeds shared configuration from local constants on first initialization:

**Phonebook Seeding:**
- All entries from `phoneBookEntries[]` (generated from `phoneBook/pb.txt`) become quick dial entries
- Makes local phonebook available to all servers through shared `CorePhoneConfig`

**DnD Settings Seeding:**
- `enabled = true` (from `kDndEnabled`)
- `startHour = 18, startMinute = 30` (from `kDndStartHour/Minute`) 
- `endHour = 8, endMinute = 30` (from `kDndEndHour/Minute`)

**Persistence:**
- **With HA Integration:** Seeded data saved to SPIFFS, prevents re-seeding on subsequent boots
- **Without HA Integration:** Re-seeds from local config on each device restart (no persistence)

This ensures every server receives the complete local configuration without manual setup.

## Development Workflows

### Standard Development Cycle
```powershell
# Build and flash firmware
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment debugHA -t upload
&"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor    # View serial output

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
- **Board:** ESP32 (configured as esp32dev in PlatformIO)
- **Modem:** A7670 GSM module via TinyGSM library
- **Storage:** SPIFFS for configuration persistence (when enabled)

## Key Patterns & Conventions

### Modern C++11 Architecture
The codebase follows modern C++11 patterns with minimal preprocessor usage:

```cpp
// Centralized configuration - always available
g_corePhoneConfig->setDndEnabled(true);
g_corePhoneConfig->addQuickDialEntry("John", "555-0123");
g_corePhoneConfig->setMaintenanceModeEnabled(true);

// Server management through factory pattern
ServerFactory::registerAllServers(_serverManager);

// Multi-server webhook handling
if (_serverManager.isWebhookEntry(number)) {
    _serverManager.executeWebhook(number);
}
```

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
    _modem.process(_state);
    _ringer.process(_state);
    _hookSwitch.process();
    _rotaryDial.process();
    _wifi.process();
    _timeManager.process(_state);
    
    // Process all servers
    _serverManager.process(_state);
}
```

### Shared Configuration Pattern
```cpp
// CorePhoneConfig provides shared storage across all servers
// Automatically seeds from local config on first initialization
class CorePhoneConfig {
public:
    void setDndEnabled(bool enabled);
    void setDndHours(int startHour, int startMinute, int endHour, int endMinute);
    void addQuickDialEntry(const String& name, const String& number);
    void addBlockedNumber(const String& number);
    bool isNumberBlocked(const String& number) const;
    void setMaintenanceModeEnabled(bool enabled);
    bool isMaintenanceModeEnabled() const;
    // Thread-safe, persistent storage via SPIFFS when available
    // Auto-seeds phonebook and DnD settings from local config on first run
};

// Global access point (architecturally guaranteed to exist)
extern CorePhoneConfig* g_corePhoneConfig;
```

### Multi-Server Webhook Pattern
```cpp
// Each server implements webhook interface
class IServer {
public:
    virtual bool hasWebhookEntry(const char* number) const { return false; }
    virtual bool executeWebhook(const char* number) const { return false; }
    // Other server methods...
};

// ServerManager aggregates webhooks across all servers
bool ServerManager::isWebhookEntry(const char *number) {
    for (auto server : _servers) {
        if (server->isEnabled() && server->hasWebhookEntry(number)) {
            return true;
        }
    }
    return false;
}
```

### Server Factory Pattern
```cpp
// Automatic server registration based on build configuration
class ServerFactory {
public:
    static void registerAllServers(ServerManager& manager) {
        // Conditionally compile only relevant servers
        #ifdef HOME_ASSISTANT_INTEGRATION
        manager.addServer(new HomeAssistantServer());
        #endif
        // Future: AndroidServer, MqttServer, etc.
    }
};
```

### Memory Management
```cpp
// Use F() macro for static strings to save RAM
Logger::infoln(F("Phone state changed to: %s"), stateString);

// Smart pointers for automatic cleanup
std::shared_ptr<CorePhoneConfig> _corePhoneConfig;

// Limit JSON buffer sizes for ESP32 constraints
const constexpr int kJsonBufferSize = 1024;
```

### Hardware Abstraction
Each component follows standard init/process pattern:
```cpp
class HardwareComponent {
public:
    void init();     // Setup hardware pins/peripherals
    void process();  // Main loop processing (non-blocking)
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
// CorePhoneConfig provides shared configuration across all servers
struct DndSettings {
    bool enabled;
    uint8_t startHour, startMinute, endHour, endMinute;
};

// Server-specific configuration handled by each server independently
// HomeAssistant example:
struct HaConfig {
    std::vector<WebhookEntry> webhooks;  // HA-specific webhooks
    String haServerUrl;                  // For webhook callbacks
    // Shared config accessed via g_corePhoneConfig
};
```

## Critical Build Dependencies

### Always Required
```ini
[env]
lib_deps = 
    https://github.com/lewisxhe/TinyGSM-fork.git#cf5c438286666e91c03762bac1825a05d1bf844a
    tzapu/WiFiManager@^2.0.17
```

### HA Integration Builds Only
```ini
[env:debugHA]
build_flags = -DHOME_ASSISTANT_INTEGRATION
lib_deps =
    ${env.lib_deps}
    ESP32Async/ESPAsyncWebServer@^3.7.9
    ESP32Async/AsyncTCP@^3.4.5
    bblanchon/ArduinoJson@^7.4.2
```

## Debugging & Testing

### WebSerial Access (when enabled)
- **URL:** `http://device-ip:32860/webserial`
- **Features:** Bidirectional serial communication via web browser
- **Memory monitoring:** Free heap displayed every 60s

### Direct API Testing
```powershell
# Test device health
curl http://device-ip/status
curl http://device-ip/stats

# Trigger actions (when HA enabled)
curl -X POST http://device-ip/action `
  -H "Content-Type: application/json" `
  -d '{"action":"call_custom","number":"1234567890"}'
```

### Common Issues
- **Memory exhaustion:** Monitor free heap, use F() macros
- **Network timeouts:** WiFiManager portal timeout is 5 minutes
- **Build failures:** Ensure correct environment selected
- **Integration mismatch:** HA integration requires `HOME_ASSISTANT_INTEGRATION` flag

### Debug Logging

```yaml
# configuration.yaml
logger:
  default: info
  logs:
    custom_components.tsuryphone: debug
```

### Health Checks

- Monitor coordinator update success in HA logs
- Check device `/stats` endpoint for memory issues
- Verify WebSocket connection status in coordinator

## Code Organization Principles

### Conditional Compilation
Minimize use of `#ifdef` guards - prefer runtime checks:
```cpp
// Minimal preprocessor usage - only for major features
#ifdef HOME_ASSISTANT_INTEGRATION
// HA-specific code
#endif

#ifdef WEB_SERIAL  
// WebSerial debug code
#endif

// Prefer runtime checks over compile-time
if (_serverManager.isWebhookEntry(number)) {
    // Multi-server webhook handling
    _serverManager.executeWebhook(number);
}
```

### Error Handling
```cpp
// Validate all inputs, especially from network
if (!data || len == 0) {
    sendError(request, "Missing data");
    return;
}

// Use RAII patterns for cleanup
class AutoCleanup {
public:
    ~AutoCleanup() { /* automatic cleanup */ }
};
```

### Logging Strategy
```cpp
// Use appropriate log levels with F() macros
Logger::infoln(F("Important state changes"));
Logger::errorln(F("Error conditions"));
Logger::debugln(F("Verbose debugging"));  // Only in debug builds
```

### Architectural Guidelines

- **Separation of Concerns:** Hardware, network, servers, and core logic in separate folders
- **Dependency Injection:** Use factory patterns and interfaces for testability
- **Shared State:** CorePhoneConfig provides centralized storage accessible to all components
- **Server Agnostic:** Core phone functionality works independently of any server implementation
- **Memory Conscious:** ESP32 constraints require careful buffer sizing and F() macro usage

When making changes, always test both HA-enabled and basic builds to ensure the integration remains truly optional and doesn't break core phone functionality.

## AI Coding Guidelines

When contributing code to this project, follow these principles:

### 1. No Backward Compatibility by Default
- **Don't add backward compatibility** unless explicitly requested by the user
- Clean implementations are preferred over maintaining legacy support
- Remove deprecated code and patterns when modernizing
- Example: When renaming methods or changing APIs, update all references without compatibility shims

### 2. Avoid Obvious Comments
- **Don't add comments that merely restate what the code does**
- Good: `// Seed DnD settings from local config constants`
- Bad: `// Set enabled to kDndEnabled` (when code shows `_dndSettings.enabled = kDndEnabled;`)
- Focus on **why** something is done, not **what** is being done
- Document business logic, architectural decisions, and non-obvious behavior
- Use meaningful variable and function names that self-document the code

### 3. Code Quality Over Convenience
- Prefer clean, maintainable code over shortcuts
- Use proper C++11 patterns and RAII where applicable
- Test both build configurations (basic + HA integration) after changes
- Follow the existing architectural patterns in the codebase

### 4. Keep Error Handling Simple
- **Don't check for impossible conditions** - If a pointer is architecturally guaranteed to exist (like `g_corePhoneConfig`), don't add null checks
- **Avoid defensive programming paranoia** - Only handle errors that can realistically occur
- **Minimize error logging noise** - Don't log every uninteresting error or validation failure
- **Focus on actionable errors** - Log errors that require user attention or indicate real problems
- **Example of unnecessary check**: `if (!g_corePhoneConfig)` when the architecture guarantees it exists
- **Example of useful check**: Validating user input or network responses that can actually fail

## Code Formatting Guidelines

Follow these formatting conventions based on the existing codebase (`main.cpp`, `modem.cpp`):

### File Structure
```cpp
// 1. Single header guard
#pragma once

// 2. Related includes first, then system includes
#include "main.h"
#include "config/corePhoneConfig.h"
#include "generated/phoneBook.h"
#include <Arduino.h>
#include <TinyGsmClient.h>

// 3. Anonymous namespace for file-local constants
namespace {
  const constexpr int kSerialBaudRate = kModemBaudRate;
  const constexpr int kCheckHardwareTimeout = 1500;
  const constexpr uint32_t kKeepAliveIntervalMs = 30000UL;
}

// 4. Class implementation
```

### Naming Conventions
```cpp
// Constants: kPascalCase with 'k' prefix
const constexpr int kDialToneDuration = 1000000;
const constexpr uint32_t kKeepAliveTimeoutMs = 5000UL;

// Private members: _camelCase with underscore prefix
bool _waitingForKeepAlive;
uint32_t _lastKeepAliveSent;
std::shared_ptr<CorePhoneConfig> _corePhoneConfig;

// Public members: camelCase (no prefix)
VolumeMode volumeMode;
CallState callState;

// Functions: camelCase
void sendCheckHardwareCommand();
bool messageAvailable() const;

// Enums: PascalCase with class scope
enum class AppState { Startup, CheckHardware, Idle };
enum class VolumeMode { Earpiece, Speaker };
```

### Spacing and Brackets
```cpp
// Function definitions: Opening brace on same line
void PhoneApp::setup() {
  Serial.begin(kSerialBaudRate);
  // ...
}

// Control structures: Space before parentheses, opening brace on same line
if (callNumber[0] != '\0' && _corePhoneConfig->isNumberBlocked(callNumber)) {
  Logger::infoln(F("Blocking call from: %s"), callNumber);
  return;
}

// Switch statements: Case labels aligned with switch
switch (_state.newAppState) {
case AppState::CheckHardware:
  processStateCheckHardware();
  break;
case AppState::Idle:
  processStateIdle();
  break;
default:
  break;
}

// Function calls: No space before parentheses
_modem.enqueueTone(Tone::DialTone, kDialToneDuration);
Logger::infoln(F("System ready!"));
```

### Parameter Alignment
```cpp
// Multi-line function calls: Align parameters or use single indentation
const DialedNumberValidationResult dialedNumberValidation =
    validateDialedNumber(dialedNumber, _corePhoneConfig.get(), &_serverManager);

// Long parameter lists: One parameter per line, aligned
sscanf(msg,
       "+CLCC: %d,%d,%d,%d,%d,\"%31[^\"]\"",
       &callId,
       &callDirection,
       &callStatus,
       &callMode,
       &callMpty,
       callNumber);
```

### Comments and Documentation
```cpp
// Single-line comments: Space after //
// Check if number is blocked using core phone config

// Multi-line explanations: Consistent formatting
// This is not the best way to do this, but I prefered it to having the state
// change inside the ringer loop, and have a designated state (e.g. AfterFirstRing)
// to handle this.

// Inline comments for non-obvious code
_isPlayingAudio = true;  // Set immediately to prevent next item from starting
```

### Variable Declarations
```cpp
// Declare variables close to first use
CallState &callState = _state.callState;
char *callNumber = callState.callNumber;

// Initialize complex objects clearly
PhoneApp::PhoneApp() : _modem(), _ringer(), _hookSwitch(), _rotaryDial(), _wifi() {
  _state.newAppState = AppState::Startup;
  _state.prevAppState = AppState::Startup;
}

// Use auto for complex iterator types, explicit types for clarity
auto it = std::find_if(_quickDialEntries.begin(), _quickDialEntries.end(), /*...*/);
const bool afterFirstRing = !prevRangAtLeastOnce && _state.callState.rangAtLeastOnce;
```

### Logging and F() Macro Usage
```cpp
// Always use F() macro for static strings to save RAM
Logger::infoln(F("TsuryPhone starting..."));
Logger::errorln(F("Cannot add quick dial entry - core config not available"));

// Format strings with proper type specifiers
Logger::infoln(F("Server setup complete - %d servers configured"), serverCount);
Logger::infoln(F("Keep-alive received after %lu ms"), millis() - _lastKeepAliveSent);

// Conditional compilation logging
#ifdef DEBUG
if (Serial.available()) {
  char c = Serial.read();
  SerialAT.write(c);
}
#endif
```

### Error Handling
```cpp
// Early returns for error conditions that can realistically occur
if (callNumber[0] == '\0') {
  return;
}

// Only check for errors that can actually happen
// DON'T: if (!g_corePhoneConfig) - architecturally guaranteed to exist
// DO: Validate user input or network responses
if (sscanf(msg, "+CLCC: %d,%d", &status, &n) != 2) {
  Logger::warnln(F("Failed to parse network response"));
  return;
}

// Log actionable errors, not every validation failure
if (dialedNumberValidation == DialedNumberValidationResult::Invalid) {
  _modem.enqueueMp3(dial_error, kInvalidNumberMp3RepeatCount);
  setState(AppState::InvalidNumber);
  // No need to log "invalid number" - user gets audio feedback
}

// Focus on errors that indicate real problems
if (!_modem.probeOK(timeout)) {
  Logger::errorln(F("Modem unreachable - rebooting MCU in 5 s"));
  ESP.restart();
}
```

### Multi-Server Development Guidelines

- **Shared Configuration**: Use `CorePhoneConfig` for settings that affect the phone itself
- **Server-Specific Configuration**: Keep webhooks, URLs, and credentials within each server
- **Webhook Aggregation**: The `ServerManager` automatically checks all servers for webhook matches
- **Clean Architecture**: No backward compatibility by default - modern, clean implementations preferred
- **Factory Pattern**: New servers register themselves via `ServerFactory::registerAllServers()`

## Multi-Server Configuration Patterns

### Shared vs Server-Specific Settings

The architecture cleanly separates concerns:

**✅ Shared Across All Servers (Phone-level config):**
- **DnD Settings**: `g_corePhoneConfig->setDndEnabled()`
- **Blocked Numbers**: `g_corePhoneConfig->addBlockedNumber()`
- **Quick Dial**: `g_corePhoneConfig->addQuickDialEntry()`
- **Maintenance Mode**: `g_corePhoneConfig->setMaintenanceModeEnabled()`

**✅ Server-Independent (Each server has its own):**
- **Webhooks**: Each server implements its own webhook storage and execution
- **Server URLs**: HA server URL, Android app endpoints, etc.
- **Authentication**: Server-specific tokens and credentials

### Adding New Servers

To add a new server (e.g., AndroidServer):

```cpp
// 1. Implement IServer interface
class AndroidServer : public IServer {
public:
    // Basic server methods
    void init() override;
    void process(const State& state) override;
    const char* getName() const override { return "Android"; }
    bool isEnabled() const override;
    
    // Webhook methods
    bool hasWebhookEntry(const char* number) const override;
    bool executeWebhook(const char* number) const override;
    
private:
    std::vector<AndroidWebhook> _androidWebhooks;  // Server-specific
};

// 2. Register in ServerFactory
#ifdef ANDROID_SERVER_INTEGRATION
manager.addServer(new AndroidServer());
#endif

// 3. Access shared config
g_corePhoneConfig->isDndEnabled();        // Shared settings
g_corePhoneConfig->isNumberBlocked(num);  // Shared blocking
```

### Webhook Aggregation Example

```cpp
// Phone dialing logic automatically checks all servers
void PhoneApp::processStateDialing() {
    // ...existing code...
    
    // Check if dialed number is a webhook across ALL servers
    if (_serverManager.isWebhookEntry(dialedNumber)) {
        if (_serverManager.executeWebhook(dialedNumber)) {
            setState(AppState::Idle);  // Webhook executed successfully
            return;
        }
    }
    
    // ...continue with normal call logic...
}
```
