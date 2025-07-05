# TsuryPhone Home Assistant Integration - Complete Implementation Guide

## Overview

I've created a comprehensive Home Assistant integration for your TsuryPhone project that exposes all phone functionality through a RESTful API and provides a full-featured HA custom component.

## What's Been Implemented

### 1. Firmware Changes

#### HTTP Server Component (`src/common/httpServer.h` & `httpServer.cpp`)
- RESTful API with JSON endpoints for all phone operations
- Persistent configuration storage using SPIFFS
- mDNS discovery support (`tsuryphone.local`)
- CORS support for web interface compatibility

#### Configuration Updates (`src/config.h`)
- Added `HOME_ASSISTANT_INTEGRATION` compile flag (optional)
- Device name configuration for mDNS

#### Main Application Integration (`src/main.h` & `main.cpp`)
- HTTP server lifecycle management
- Callback functions for HA integration actions
- State synchronization with HA server
- Call screening for blocked numbers

#### State Management Updates (`src/common/state.h`)
- Extended state structure to support HA-configurable DnD hours
- HA override flags for enhanced control

#### Time Manager Updates (`src/common/timeManager.cpp`)
- Integration with HA-configured DnD hours
- Support for HA DnD overrides

#### Build Configuration (`platformio.ini`)
- New build environments: `debugHA` and `debugWebSerialHA`
- Required library dependencies (ArduinoJson, AsyncWebServer, etc.)

### 2. Home Assistant Custom Component

#### Full Platform Support
- **Sensors**: Phone state, call info, statistics, hardware info
- **Binary Sensors**: Call status, DnD status, WiFi connection, call waiting
- **Buttons**: Hangup, reset, reboot actions
- **Switches**: DnD enable/disable
- **Numbers**: DnD hour configuration
- **Select**: Quick-dial from phonebook
- **Services**: Advanced phonebook and screening management

#### Device Discovery & Setup
- Automatic device validation during setup
- Configuration flow with error handling
- Unique device identification using MAC address

## API Endpoints

### Status & Information
- `GET /` - Device information and API discovery
- `GET /status` - Current phone state and call information
- `GET /stats` - System statistics and hardware info
- `GET /dnd` - Do Not Disturb configuration
- `GET /phonebook` - Phonebook entries
- `GET /screened` - Screened (blocked) numbers

### Actions
- `POST /action/call` - Make a call (`number` parameter)
- `POST /action/hangup` - Hang up current call
- `POST /action/reset` - Reset device
- `POST /action/reboot` - Reboot device

### Configuration
- `POST /dnd` - Configure Do Not Disturb (enabled, hours)
- `POST /phonebook` - Add/remove phonebook entries
- `POST /screened` - Add/remove screened numbers

## Installation Instructions

### Step 1: Enable Integration in Firmware

1. Open `src/config.h`
2. Uncomment this line:
   ```cpp
   //#define HOME_ASSISTANT_INTEGRATION
   ```
   to:
   ```cpp
   #define HOME_ASSISTANT_INTEGRATION
   ```

3. Build and upload using one of these environments:
   - `debugHA` - HA integration only
   - `debugWebSerialHA` - HA integration + WebSerial

### Step 2: Install Home Assistant Component

1. Copy the entire `home_assistant/custom_components/tsuryphone` folder to your Home Assistant `custom_components` directory:
   ```
   /config/custom_components/tsuryphone/
   ```

2. Restart Home Assistant

3. Go to **Settings** → **Devices & Services** → **Add Integration**

4. Search for "TsuryPhone" and select it

5. Enter your TsuryPhone's IP address and port (default: 80)

### Step 3: Configure and Use

Once installed, you'll have access to:

#### Entities Created
- `sensor.tsuryphone_state` - Current phone state
- `sensor.tsuryphone_uptime` - Device uptime
- `sensor.tsuryphone_free_heap` - Memory usage
- `sensor.tsuryphone_wifi_rssi` - WiFi signal
- `sensor.tsuryphone_total_calls` - Call statistics
- `binary_sensor.tsuryphone_call_active` - Call in progress
- `binary_sensor.tsuryphone_dnd_active` - DnD status
- `binary_sensor.tsuryphone_wifi_connected` - WiFi status
- `button.tsuryphone_hangup` - Hang up button
- `button.tsuryphone_reset` - Reset button
- `switch.tsuryphone_dnd` - DnD toggle
- `number.tsuryphone_dnd_start_hour` - DnD start time
- `select.tsuryphone_call_phonebook` - Quick dial

#### Services Available
- `tsuryphone.call_number` - Make a call
- `tsuryphone.add_phonebook_entry` - Add contact
- `tsuryphone.remove_phonebook_entry` - Remove contact
- `tsuryphone.add_screened_number` - Block number
- `tsuryphone.remove_screened_number` - Unblock number

## Features Integrated into Core Phone Functions

### Call Screening
- Numbers added to the screened list via HA are automatically blocked
- Incoming calls from screened numbers are immediately hung up
- Screening happens before the phone rings

### Do Not Disturb
- HA can override the default DnD settings from config.h
- DnD hours can be configured through HA number entities
- HA switch can force DnD on/off regardless of time

### Phonebook Integration
- HA-managed phonebook entries work seamlessly with the dial pad
- Quick-dial select entity for easy calling through HA
- Phonebook changes are persistent and survive reboots

### State Monitoring
- Real-time phone state updates in HA
- Call statistics tracking
- Hardware monitoring (memory, WiFi, etc.)

## Usage Examples

### Automation: Notify on Incoming Call
```yaml
automation:
  - alias: "Phone Call Notification"
    trigger:
      - platform: state
        entity_id: sensor.tsuryphone_state
        to: "IncomingCall"
    action:
      - service: notify.mobile_app_your_phone
        data:
          title: "Incoming Call"
          message: "{{ state_attr('sensor.tsuryphone_state', 'call_number') }}"
```

### Automation: Auto DnD at Bedtime
```yaml
automation:
  - alias: "Auto DnD Bedtime"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.tsuryphone_dnd
```

### Script: Emergency Call
```yaml
script:
  emergency_call:
    alias: "Call Emergency"
    sequence:
      - service: tsuryphone.call_number
        data:
          device_id: "tsuryphone"
          number: "911"
```

## Technical Notes

### Optional Integration
- The integration is completely optional - use `#define HOME_ASSISTANT_INTEGRATION` to enable
- No impact on phone functionality when disabled
- Minimal memory overhead when enabled

### Memory Management
- Uses SPIFFS for persistent configuration storage
- JSON configuration with reasonable size limits
- Efficient memory usage with ArduinoJson library

### Network Considerations
- HTTP server runs on port 80 (configurable)
- mDNS for easy discovery (`tsuryphone.local`)
- CORS headers for web interface compatibility
- Timeout handling for reliable operation

### Security
- Local network only (no external access)
- Input validation on all API endpoints
- Error handling and logging

## Troubleshooting

### Build Issues
- Ensure ArduinoJson library is installed
- Check that `HOME_ASSISTANT_INTEGRATION` is defined
- Use the correct build environment (`debugHA` or `debugWebSerialHA`)

### Connection Issues
- Verify device IP address in HA integration setup
- Check that device is on same network as HA
- Ensure port 80 is accessible (no firewall blocking)

### Entity Issues
- Restart HA after firmware updates
- Check HA logs for integration errors
- Verify device is responding at configured IP

This comprehensive integration provides everything needed for full Home Assistant control of your TsuryPhone while maintaining the core functionality and allowing the integration to be completely optional.
