# GreenHouse8 Motor Control Enhancement

## Version 1.9.0 (V17)

This release adds comprehensive motor control features to the GreenHouse8 automation system.

## New Features

### 1. Global Motor Auto Control
- **Auto Mode**: Enable automatic cycling of all motors between forward and reverse
- **Configurable Timing**: Set independent forward and reverse duration times
- **Safety Lock**: Prevents manual motor control when global auto mode is active
- **Persistent Storage**: Configuration saved to NVS (Non-Volatile Storage)

### 2. Individual Motor Control
- Manual control for each of the 4 motors independently
- Three states per motor: OFF (0), FORWARD (1), REVERSE (2)
- Safety lock integration - blocked when global auto is enabled

### 3. Group Control Features
- **All Motors**: Control all 4 motors simultaneously (FW/RE/OFF)
- **Motors 1-2**: Control motors 1 and 2 together
- **Motors 3-4**: Control motors 3 and 4 together

### 4. Safety Features
- **Interlock Protection**: Prevents simultaneous activation of FW and RE pins (short circuit prevention)
- **Safety Lock**: Manual control disabled during global auto mode
- **Delay-based Protection**: 100ms delay between direction changes

### 5. ThingsBoard Integration
- Full RPC method support for remote control
- Real-time telemetry reporting
- HTML widget (motor_widget_v7.html) for visual control panel

## Files

### Source Code
- **GreenHouse8_V_17.ino** - Main Arduino sketch with all motor control features

### ThingsBoard Widget
- **motor_widget_v7.html** - HTML/JavaScript widget for ThingsBoard dashboard

### Configuration
- **config.json** - OTA update configuration

## RPC Methods

### Global Motor Auto Control
- `set_global_motor_auto` - Enable/disable global auto mode
  - Params: `{ "enabled": true/false }`
- `set_global_fw_time` - Set forward duration in seconds
  - Params: `{ "time": seconds }`
- `set_global_re_time` - Set reverse duration in seconds
  - Params: `{ "time": seconds }`

### Individual Motor Control
- `set_motor_1_status` - Set motor 1 status (0=OFF, 1=FW, 2=RE)
  - Params: `{ "status": 0/1/2 }`
- `set_motor_2_status` - Set motor 2 status
- `set_motor_3_status` - Set motor 3 status
- `set_motor_4_status` - Set motor 4 status

### Legacy Methods (with safety lock)
- `set_motor_1_fw`, `set_motor_1_re`
- `set_motor_2_fw`, `set_motor_2_re`
- `set_motor_3_fw`, `set_motor_3_re`
- `set_motor_4_fw`, `set_motor_4_re`

### Group Control Methods
- `set_all_motors_fw` - All motors forward
- `set_all_motors_re` - All motors reverse
- `set_all_motors_off` - All motors off
- `set_motors_12_fw` - Motors 1-2 forward
- `set_motors_12_re` - Motors 1-2 reverse
- `set_motors_12_off` - Motors 1-2 off
- `set_motors_34_fw` - Motors 3-4 forward
- `set_motors_34_re` - Motors 3-4 reverse
- `set_motors_34_off` - Motors 3-4 off

## Implementation Details

### Global Variables Added
- `globalMotorAuto` - Global auto mode state
- `globalFwTime` - Forward duration (milliseconds)
- `globalReTime` - Reverse duration (milliseconds)
- `lastGlobalAction` - Timestamp of last action
- `lastGlobalMotorState` - Current state in auto cycle

### Helper Functions Added
- `loadGlobalConfig()` - Load settings from NVS
- `saveGlobalConfig()` - Save settings to NVS
- `controlMotorStatus(motorNum, status)` - Control motor with safety checks
- `setMotorPins(fwPin, rePin, status)` - Low-level motor control with interlock

### Modified Functions
- `checkAutoTimers()` - Now handles both fan and motor auto timing
- `callback()` - Enhanced with new RPC methods and safety lock logic
- `setup()` - Added `loadGlobalConfig()` call

## Hardware Configuration

The default pin configuration (adjust as needed):
- Motor 1: FW=16, RE=17
- Motor 2: FW=18, RE=19
- Motor 3: FW=20, RE=21
- Motor 4: FW=22, RE=23
- Fan 1: 25
- Fan 2: 26

## Installation

### Arduino IDE
1. Copy `GreenHouse8_V_17.ino` to your Arduino sketch folder
2. Install required libraries:
   - WiFi
   - PubSubClient
   - ArduinoJson
   - Preferences
   - HTTPClient
3. Configure WiFi and ThingsBoard credentials in the code
4. Upload to ESP32-S3

### ThingsBoard Widget
1. Go to ThingsBoard dashboard
2. Add new widget (HTML Card)
3. Paste contents of `motor_widget_v7.html`
4. Configure data keys:
   - motor1_status, motor2_status, motor3_status, motor4_status
   - global_motor_auto, global_fw_time, global_re_time

## Safety Notes

⚠️ **Important Safety Features:**
1. **Interlock Protection**: The system automatically prevents FW and RE from being active simultaneously
2. **Safety Lock**: Global auto mode prevents manual control to avoid conflicts
3. **100ms Delay**: Built-in delay between direction changes protects hardware

## Testing

To test the implementation:
1. Upload code to ESP32-S3
2. Monitor serial output for debug messages
3. Test manual motor control via RPC
4. Enable global auto mode and verify cycling
5. Verify safety lock prevents manual control during auto mode
6. Test group control functions
7. Verify interlock protection

## Telemetry Data

The system sends the following telemetry every 5 seconds:
- `motor1_status`, `motor2_status`, `motor3_status`, `motor4_status`
- `global_motor_auto`, `global_fw_time`, `global_re_time`
- `fan1_state`, `fan1_auto`, `fan2_state`, `fan2_auto`
- `wifi_rssi`, `uptime`

## Version History

- **v1.9.0 (V17)** - Motor control enhancement with auto mode, group control, and safety features
- **v1.8.8.2-rev2 (V16)** - Previous version with basic motor control
