# TsuryPhone Home Assistant Integration

[![hacs_badge](https://img.shields.io/badge/HACS-Custom-41BDF5.svg)](https://github.com/hacs/integration)

This custom integration allows you to control and monitor your TsuryPhone device through Home Assistant.

> **💡 Easy Installation**: This integration can be installed via [HACS](https://hacs.xyz/) for easy installation and automatic updates!

## 🚀 Installation

### Via HACS (Recommended)

1. **Install HACS** if you haven't already
2. **Add Custom Repository**:
   - Go to HACS → Integrations
   - Click ⋮ (menu) → Custom repositories
   - Repository: `https://github.com/Tsury/TsuryPhone`
   - Category: Integration
3. **Download Integration**:
   - Search for "TsuryPhone" in HACS
   - Click Download
   - Restart Home Assistant
4. **Add Integration**:
   - Go to Settings → Devices & Services
   - Click Add Integration → TsuryPhone
   - Enter your device IP address

### Manual Installation

1. Copy the `home_assistant/custom_components/tsuryphone` folder to your Home Assistant `custom_components` directory
2. Restart Home Assistant
3. Go to **Settings** > **Devices & Services**
4. Click **Add Integration** and search for "TsuryPhone"
5. Enter your TsuryPhone's IP address and port (default: 80)

### 4. Verify Connection

Once added, you should see:
- A TsuryPhone device in your devices list
- All sensors, switches, and controls available
- Real-time updates of phone status

## ✨ Features

### Monitoring
- **Phone State**: Current state of the phone (Idle, InCall, Dialing, etc.)
- **Call Information**: Active call number, call ID, call waiting status
- **System Statistics**: Uptime, free heap memory, total calls, resets
- **WiFi Status**: Connection status, IP address, RSSI signal strength
- **Hardware Info**: CPU frequency, flash size, sketch size

### Controls
- **Call Management**: Make calls, hang up active calls
- **Do Not Disturb**: Enable/disable DnD and configure hours
- **Device Management**: Reset or reboot the device
- **Phonebook**: Add/remove phonebook entries via services
- **Call Screening**: Block unwanted numbers

### Entities Created

#### Sensors
- `sensor.tsuryphone_state` - Current phone state
- `sensor.tsuryphone_uptime` - Device uptime
- `sensor.tsuryphone_free_heap` - Available memory
- `sensor.tsuryphone_wifi_rssi` - WiFi signal strength
- `sensor.tsuryphone_total_calls` - Total call count
- `sensor.tsuryphone_incoming_calls` - Incoming call count
- `sensor.tsuryphone_outgoing_calls` - Outgoing call count
- `sensor.tsuryphone_resets` - Device reset count
- `sensor.tsuryphone_call_number` - Current call number
- `sensor.tsuryphone_call_id` - Current call ID
- `sensor.tsuryphone_cpu_freq` - CPU frequency
- `sensor.tsuryphone_flash_size` - Flash memory size
- `sensor.tsuryphone_sketch_size` - Sketch size

#### Binary Sensors
- `binary_sensor.tsuryphone_call_active` - Call in progress
- `binary_sensor.tsuryphone_dnd_active` - Do Not Disturb status
- `binary_sensor.tsuryphone_wifi_connected` - WiFi connection
- `binary_sensor.tsuryphone_call_waiting` - Call waiting status

#### Buttons
- `button.tsuryphone_hangup` - Hang up active call
- `button.tsuryphone_reset` - Reset device
- `button.tsuryphone_reboot` - Reboot device
- `button.tsuryphone_ring` - Ring the device (5 seconds)

#### Switches
- `switch.tsuryphone_dnd` - Enable/disable Do Not Disturb

#### Numbers
- `number.tsuryphone_dnd_start_hour` - DnD start hour
- `number.tsuryphone_dnd_start_minute` - DnD start minute
- `number.tsuryphone_dnd_end_hour` - DnD end hour
- `number.tsuryphone_dnd_end_minute` - DnD end minute

#### Selects
- `select.tsuryphone_call_phonebook` - Call contacts from phonebook

### Services

#### `tsuryphone.call_number`
Make a call to a specific number.
```yaml
service: tsuryphone.call_number
data:
  device_id: "tsuryphone"
  number: "1234567890"
```

#### `tsuryphone.ring_device`
Ring the device for a specified duration.
```yaml
service: tsuryphone.ring_device
data:
  device_id: "tsuryphone"
  duration: 10000  # Ring for 10 seconds (optional, default: 5000ms)
```

#### `tsuryphone.add_phonebook_entry`
Add a new phonebook entry.
```yaml
service: tsuryphone.add_phonebook_entry
data:
  device_id: "tsuryphone"
  name: "John Doe"
  number: "1234567890"
```

#### `tsuryphone.remove_phonebook_entry`
Remove a phonebook entry.
```yaml
service: tsuryphone.remove_phonebook_entry
data:
  device_id: "tsuryphone"
  name: "John Doe"
```

#### `tsuryphone.add_screened_number`
Block a phone number.
```yaml
service: tsuryphone.add_screened_number
data:
  device_id: "tsuryphone"
  number: "1234567890"
```

#### `tsuryphone.remove_screened_number`
Unblock a phone number.
```yaml
service: tsuryphone.remove_screened_number
data:
  device_id: "tsuryphone"
  number: "1234567890"
```

## Installation

### 1. Enable the Integration in Firmware

First, enable the Home Assistant integration in your TsuryPhone firmware:

1. Build and upload using an environment with the `HA` suffix:
   ```bash
   # Debug build with HA
   pio run -e debugHA -t upload
   
   # Release build with HA  
   pio run -e releaseHA -t upload
   ```

### 2. Install via HACS (Recommended)

This integration is designed for easy installation via HACS:

1. **Add to HACS**: 
   - HACS → Integrations → ⋮ → Custom repositories
   - Repository: `https://github.com/Tsury/TsuryPhone` 
   - Category: Integration
2. **Download**: Search "TsuryPhone" and click Download
3. **Restart** Home Assistant
4. **Add Integration**: Settings → Devices & Services → Add Integration → TsuryPhone

### 3. Manual Installation (Alternative)

If you prefer manual installation:

1. Copy the `home_assistant/custom_components/tsuryphone` folder to your Home Assistant `custom_components` directory
2. Restart Home Assistant
3. Go to **Settings** > **Devices & Services**
4. Click **Add Integration** and search for "TsuryPhone"
5. Enter your TsuryPhone's IP address and port (default: 80)

### 4. Verify Connection

### 3. Verify Connection

Once added, you should see:
- A TsuryPhone device in your devices list
- All sensors, switches, and controls available
- Real-time updates of phone status

## Configuration

### Device Configuration
The device can be configured via the web interface at `http://[device-ip]/` or through Home Assistant entities.

### Automation Examples

#### Notify on Incoming Call
```yaml
automation:
  - alias: "TsuryPhone Incoming Call Notification"
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

#### Auto-enable DnD at Night
```yaml
automation:
  - alias: "TsuryPhone Auto DnD"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.tsuryphone_dnd
```

#### Call Emergency Contact
```yaml
automation:
  - alias: "Emergency Call"
    trigger:
      - platform: state
        entity_id: input_boolean.emergency
        to: "on"
    action:
      - service: tsuryphone.call_number
        data:
          device_id: "tsuryphone"
          number: "911"
```

#### Ring Phone as Doorbell
```yaml
automation:
  - alias: "Ring Phone When Doorbell Pressed"
    trigger:
      - platform: state
        entity_id: binary_sensor.doorbell
        to: "on"
    action:
      - service: tsuryphone.ring_device
        data:
          device_id: "tsuryphone"
          duration: 3000  # Ring for 3 seconds
```

## Troubleshooting

### Connection Issues
- Ensure the TsuryPhone is connected to the same network as Home Assistant
- Check that the HOME_ASSISTANT_INTEGRATION flag is enabled in firmware
- Verify the IP address and port in the integration configuration

### Missing Entities
- Restart Home Assistant after firmware updates
- Check Home Assistant logs for any errors
- Ensure the device is responding at the configured IP/port

### API Endpoints
The integration uses these REST API endpoints:
- `GET /status` - Current device status
- `GET /stats` - Device statistics
- `GET /dnd` - Do Not Disturb configuration
- `GET /phonebook` - Phonebook entries
- `GET /screened` - Screened numbers
- `POST /action/call` - Make a call
- `POST /action/hangup` - Hang up
- `POST /action/reset` - Reset device
- `POST /action/reboot` - Reboot device
- `POST /action/ring` - Ring device for specified duration

## Support

For issues and feature requests, please check the project repository.

## 🎯 **Summary of Changes - HA Integration Priority**

I've successfully modified the codebase so that when Home Assistant integration is enabled, the local DnD and Phonebook settings are completely ignored in favor of HA settings:

### ✅ **Changes Made:**

#### **1. Phonebook Integration (`src/common/phoneBook.*`)**
- **Added runtime functions**: `isPhoneBookEntry_Runtime()`, `isPartialOfFullPhoneBookEntry_Runtime()`, `getPhoneBookNumberForEntry_Runtime()`
- **Conditional logic**: When `HOME_ASSISTANT_INTEGRATION` is defined, these functions call HA phonebook functions instead of local ones
- **Updated validation**: `validateDialedNumber()` now uses runtime functions, so number validation works with HA phonebook

#### **2. HA Phonebook Functions (`src/common/homeAssistantServer.*`)**
- **Added HA phonebook functions**: `isHaPhoneBookEntry()`, `isPartialOfHaPhoneBookEntry()`, `getHaPhoneBookNumberForEntry()`
- **Dynamic lookup**: These functions search the HA `haConfig.phoneBook` vector instead of the static generated phonebook
- **Memory safe**: Uses static String for cached results to ensure pointer validity

#### **3. DnD Integration (`src/common/timeManager.cpp`)**
- **HA-only logic**: When HA integration is enabled, completely ignores local DnD settings from `config.h`
- **HA DnD priority**: Uses HA configuration for enabled/disabled state and time windows
- **Fallback logic**: If HA DnD is disabled, sets `state.isDnd = false` regardless of time
- **Override support**: Maintains support for HA force-enable via `state.haDndOverride`

#### **4. Main App Updates (`src/main.cpp`)**
- **Runtime phonebook**: Changed from `isPhoneBookEntry()` to `isPhoneBookEntry_Runtime()`
- **Consistent behavior**: Now uses the same phonebook source for both validation and number lookup

### 🔄 **Behavior Changes:**

**When `HOME_ASSISTANT_INTEGRATION` is defined:**
- ✅ **Phonebook**: Only HA phonebook entries are recognized and used
- ✅ **DnD**: Only HA DnD settings control Do Not Disturb functionality  
- ✅ **Validation**: Number validation uses HA phonebook for entry detection
- ✅ **Call routing**: Phonebook number lookup uses HA phonebook

**When `HOME_ASSISTANT_INTEGRATION` is NOT defined:**
- ✅ **Phonebook**: Uses local generated phonebook (no change)
- ✅ **DnD**: Uses local config.h DnD settings (no change)
- ✅ **Backward compatibility**: All existing functionality preserved

### 🎮 **User Experience:**

**Home Assistant Users:**
- Can manage phonebook entirely through HA services
- DnD is controlled exclusively through HA entities
- Local phonebook entries are ignored
- Real-time updates when HA settings change

**Non-HA Users:**  
- No impact - uses local phonebook and DnD as before
- No memory overhead from HA integration

### 🛡️ **Safety & Compatibility:**
- ✅ **Compile-time safe**: All HA code properly guarded with `#ifdef`
- ✅ **Memory efficient**: HA phonebook stored in dynamic vector
- ✅ **Thread safe**: Static cached results for C string returns
- ✅ **Fallback ready**: Graceful handling when HA phonebook is empty

The integration now provides a clean separation where HA users get full control over phonebook and DnD via Home Assistant, while preserving all existing functionality for non-HA builds! 🎉
