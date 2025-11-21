# GreenHouse8 Motor Control System Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      ThingsBoard Dashboard                       │
│                   (motor_widget_v7.html)                         │
│                                                                   │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐          │
│  │  Individual │  │ Global Auto  │  │ Group Control │          │
│  │   Controls  │  │    Mode      │  │   Buttons     │          │
│  └─────────────┘  └──────────────┘  └───────────────┘          │
└────────────────────────────┬──────────────────────────────────────┘
                             │ MQTT / RPC Commands
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                    ESP32-S3 Device                                 │
│                  (GreenHouse8_V_17.ino)                            │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │                   callback() Function                     │    │
│  │  Receives RPC commands and routes to appropriate handler │    │
│  └────────────────────┬─────────────────────────────────────┘    │
│                       │                                            │
│       ┌───────────────┼───────────────┐                           │
│       │               │               │                           │
│  ┌────▼─────┐  ┌─────▼──────┐  ┌────▼─────┐                     │
│  │ Global   │  │ Individual │  │  Group   │                     │
│  │ Auto RPC │  │ Motor RPC  │  │ Control  │                     │
│  │ Methods  │  │  Methods   │  │   RPC    │                     │
│  └────┬─────┘  └─────┬──────┘  └────┬─────┘                     │
│       │              │               │                           │
│       │              └───────┬───────┘                           │
│       │                      │                                   │
│  ┌────▼──────────────────────▼─────────────────┐                │
│  │        controlMotorStatus()                  │                │
│  │   (with Safety Lock Check)                   │                │
│  │                                               │                │
│  │  if (globalMotorAuto) {                      │                │
│  │    // Block manual control                   │                │
│  │    return;                                    │                │
│  │  }                                            │                │
│  └────────────────────┬──────────────────────────┘                │
│                       │                                            │
│  ┌────────────────────▼──────────────────────────┐                │
│  │          setMotorPins()                       │                │
│  │     (with Interlock Protection)               │                │
│  │                                                │                │
│  │  // Turn OFF both pins first                  │                │
│  │  digitalWrite(fwPin, LOW);                    │                │
│  │  digitalWrite(rePin, LOW);                    │                │
│  │  delay(100); // Safety delay                  │                │
│  │  // Then activate desired pin                 │                │
│  └────────────────────┬──────────────────────────┘                │
│                       │                                            │
│  ┌────────────────────▼──────────────────────────┐                │
│  │          checkAutoTimers()                    │                │
│  │   (Called in main loop)                       │                │
│  │                                                │                │
│  │  if (globalMotorAuto) {                       │                │
│  │    if (lastGlobalMotorState == 1) {           │                │
│  │      if (time >= globalFwTime) {              │                │
│  │        // Switch all to REVERSE               │                │
│  │      }                                         │                │
│  │    } else if (lastGlobalMotorState == 2) {    │                │
│  │      if (time >= globalReTime) {              │                │
│  │        // Switch all to FORWARD               │                │
│  │      }                                         │                │
│  │    }                                           │                │
│  │  }                                             │                │
│  └────────────────────┬──────────────────────────┘                │
│                       │                                            │
│  ┌────────────────────▼──────────────────────────┐                │
│  │       Configuration Storage (NVS)             │                │
│  │  - globalMotorAuto                            │                │
│  │  - globalFwTime                               │                │
│  │  - globalReTime                               │                │
│  │  - fan settings                               │                │
│  └───────────────────────────────────────────────┘                │
│                                                                    │
└────────────────────────────┬───────────────────────────────────────┘
                             │
                   ┌─────────┴─────────┐
                   │                   │
          ┌────────▼────────┐  ┌──────▼──────┐
          │  Motor 1 & 2    │  │ Motor 3 & 4 │
          │  FW/RE Relays   │  │ FW/RE Relays│
          └─────────────────┘  └─────────────┘
```

## Safety Flow

```
Manual Control Attempt → controlMotorStatus() → Check globalMotorAuto
                                                         │
                                                         ├─ TRUE → Block & Return
                                                         │
                                                         └─ FALSE → Proceed
                                                                      │
                                                                      ▼
                                                            setMotorPins()
                                                                      │
                                                                      ├─ Turn OFF both pins
                                                                      ├─ Wait 10ms
                                                                      └─ Activate desired pin
```

## Auto Mode Flow

```
Global Auto Enabled → checkAutoTimers() → Check lastGlobalMotorState
                                                    │
                                    ┌───────────────┼───────────────┐
                                    │               │               │
                              State = 0         State = 1      State = 2
                              (Initial)        (Forward)      (Reverse)
                                    │               │               │
                                    │               │               │
                              Set all FW      Wait FwTime     Wait ReTime
                                    │               │               │
                                    │               ▼               │
                                    │         Set all RE            │
                                    │               │               │
                                    └───────────────┴───────────────┘
                                                    │
                                                    ▼
                                            Update Motors via
                                            setMotorPins()
```

## Data Flow

```
┌──────────────┐
│ ThingsBoard  │
│   Widget     │
└──────┬───────┘
       │ RPC Command
       ▼
┌──────────────┐
│    MQTT      │
│   Broker     │
└──────┬───────┘
       │
       ▼
┌──────────────┐     ┌─────────────┐
│  callback()  │────▶│ Update Vars │
│   Function   │     └─────────────┘
└──────┬───────┘
       │
       ▼
┌──────────────┐     ┌─────────────┐
│ Control Func │────▶│ Save to NVS │
└──────┬───────┘     └─────────────┘
       │
       ▼
┌──────────────┐
│ Set Hardware │
│     Pins     │
└──────┬───────┘
       │
       ▼
┌──────────────┐     ┌─────────────┐
│ Send         │────▶│ ThingsBoard │
│ Telemetry    │     │  Dashboard  │
└──────────────┘     └─────────────┘
```

## Key Safety Features

1. **Interlock Protection**
   - Both FW and RE pins are turned OFF before any change
   - 10ms delay ensures safe state transition
   - Prevents short circuit damage

2. **Safety Lock**
   - Manual control blocked when `globalMotorAuto == true`
   - Prevents conflicts between manual and auto control
   - Clear error message in serial log

3. **State Persistence**
   - All configurations saved to NVS
   - Survives power cycles
   - Auto-restored on boot

4. **Robust Control**
   - Status checked before every operation
   - Clear serial debug messages
   - Telemetry for monitoring
