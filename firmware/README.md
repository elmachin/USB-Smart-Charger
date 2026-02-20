# USB Smart Charger Firmware

Firmware for PIC16F19155 microcontroller managing 4-port intelligent USB charging with per-port current monitoring and automatic shutoff.

---

## Overview

This firmware implements a zero-phantom-power USB charging system that completely cuts power to devices after they finish charging, preventing LED flashing, trickle charging, and standby power consumption.

**Key Features:**
- Per-port USB CC line detection for plug/unplug events
- INA138-based current sensing on each port
- Configurable low-current shutdown threshold
- Persistence timer to prevent false shutoffs
- Boot-time visual LED feedback
- Automatic re-enable only on detach/reattach cycle

---

## Hardware Requirements

**Microcontroller:** PIC16F19155

**Pin Assignments:**

| Function | Pin | Port | Description |
|----------|-----|------|-------------|
| **Port 1 Enable** | RB4 | Output | Power control for USB Port 1 |
| **Port 2 Enable** | RB5 | Output | Power control for USB Port 2 |
| **Port 3 Enable** | RA0 | Output | Power control for USB Port 3 |
| **Port 4 Enable** | RA1 | Output | Power control for USB Port 4 |
| **Port 1 Current** | RB3 (ANB3) | Analog Input | INA138 Vout for Port 1 |
| **Port 2 Current** | RB2 (ANB2) | Analog Input | INA138 Vout for Port 2 |
| **Port 3 Current** | RA3 (ANA3) | Analog Input | INA138 Vout for Port 3 |
| **Port 4 Current** | RA2 (ANA2) | Analog Input | INA138 Vout for Port 4 |
| **Port 1 CC1** | RC7 | Digital Input | USB Port 1 CC line 1 |
| **Port 1 CC2** | RB1 | Digital Input | USB Port 1 CC line 2 |
| **Port 2 CC1** | RC4 | Digital Input | USB Port 2 CC line 1 |
| **Port 2 CC2** | RC6 | Digital Input | USB Port 2 CC line 2 |
| **Port 3 CC1** | RC2 | Digital Input | USB Port 3 CC line 1 |
| **Port 3 CC2** | RC3 | Digital Input | USB Port 3 CC line 2 |
| **Port 4 CC1** | RA4 | Digital Input | USB Port 4 CC line 1 |
| **Port 4 CC2** | RA5 | Digital Input | USB Port 4 CC line 2 |

**ADC Configuration:**
- Reference: FVR 4.096V
- Resolution: 10-bit (0-1023)
- Conversion mode: Single conversion per channel

---

## Current Sensing

**INA138 High-Side Current Monitor:**
- Sense resistor (Rs): 0.5Ω
- Load resistor (Rl): 24kΩ
- Output voltage: Vout = (Iload × Rs) × (Rl / 1000)

**Current Calculation:**
```c
Iload (mA) = Vout (mV) / (Rs × Rl/1000)
Iload (mA) = Vout (mV) / (0.5 × 24) = Vout (mV) / 12
```

**Calibration Factor:** 1.2× applied to compensate for component tolerances

**Example:**
- 10mA load current
- 0.5Ω × 10mA = 5mV across sense resistor
- INA138 gain = 24
- Vout = 5mV × 24 = 120mV
- ADC reading: ~30 counts (120mV / 4096mV × 1024)

---

## Configuration Parameters

Located at top of `main.c`:
```c
// INA138 hardware values
#define RS_OHMS         0.5f        // Sense resistor value
#define RL_KOHMS        24.0f       // Load resistor value
#define CALIBRATION_FACTOR  1.4f    // Empirically determined

// Operational thresholds
#define LOW_CURRENT_MA      10.0f   // Shutdown threshold
#define PERSIST_MS          3000    // 3 second persistence timer

// ADC reference
#define VREF_mV         4096.0f     // FVR voltage
#define ADC_RESOLUTION  1024        // 10-bit ADC
```

**Adjustable Parameters:**

| Parameter | Default | Purpose | Recommended Range |
|-----------|---------|---------|-------------------|
| `LOW_CURRENT_MA` | 10.0 | Current below which port shuts off | 5-50mA |
| `PERSIST_MS` | 3000 | Time below threshold before shutdown | 1000-5000ms |
| `CALIBRATION_FACTOR` | 1.4 | Component tolerance compensation | Measure on hardware |
| `BLINK_DELAY_MS` | 500 | Boot LED flash duration | 200-1000ms |

---

## Operating Logic

### Boot Sequence

1. **System Initialization**
   - Configure all pins (GPIO, ADC, enables)
   - Set up ADC with FVR reference
   - 100ms stabilization delay

2. **Visual Feedback**
   - Flash all 4 port LEDs twice (on/off cycles)
   - Final flash leaves all ports ON
   - Total boot sequence: ~3 seconds

3. **Initial Port Configuration**
   - Check CC lines on all 4 ports
   - Enable ports with devices already attached
   - Disable ports with nothing connected
   - Initialize state tracking for each port

### Main Loop (100ms cycle time)

**For each port (1-4), execute in order:**

#### 1. CC Line Detection
```
Read CC1 and CC2 pins
Device attached if EITHER CC line is LOW (pulled down by device)

IF (was detached AND now attached):
    → Turn port ON
    → Mark as enabled
    → Reset low-current timer

ELSE IF (was attached AND now detached):
    → Turn port OFF immediately
    → Mark as disabled
    → Reset low-current timer
```

#### 2. Current Monitoring (only if port enabled)
```
Read ADC on current sense channel
Convert to milliamps using INA138 formula

IF (current < LOW_CURRENT_MA):
    Increment low_timer by 100ms
    
    IF (low_timer >= PERSIST_MS):
        → Turn port OFF
        → Mark as disabled
        → Reset timer
        
ELSE (current >= LOW_CURRENT_MA):
    → Reset low_timer to 0 (device still charging)
```

#### 3. State Update
```
Save current CC state for next cycle comparison
```

---

## State Machine

**Port States:**
```
┌─────────────┐
│   DETACHED  │ ← No device connected, power OFF
└──────┬──────┘
       │ CC attach detected
       ↓
┌─────────────┐
│   CHARGING  │ ← Device connected, power ON, current > threshold
└──────┬──────┘
       │ Current < threshold for PERSIST_MS
       ↓
┌─────────────┐
│  COMPLETE   │ ← Device full, power OFF (still physically connected)
└──────┬──────┘
       │ CC detach detected
       ↓
┌─────────────┐
│   DETACHED  │ ← Device removed, ready for next device
└─────────────┘
```

**Critical:** Port will NOT re-enable from COMPLETE state unless device is physically unplugged and replugged (CC detach→attach cycle).

---

## Timing Characteristics

| Event | Response Time | Notes |
|-------|---------------|-------|
| Device plug-in | 100-200ms | Next main loop cycle |
| Device unplug | 100-200ms | Immediate shutoff on CC change |
| Charge complete shutoff | 3.0-3.1s | After current drops below threshold |
| Boot sequence | ~3s | Visual feedback duration |
| Main loop cycle | 100ms | Per-port update rate |

---

## Typical Use Case: Phone Charging

**Timeline:**
```
T=0s:       User plugs in phone
T=0.1s:     CC lines detected, port enabled, LED on
T=0-90min:  Phone charging, current 500-1500mA
T=90min:    Phone reaches 100%, current drops to ~5mA
T=90:00:    Low current detected, timer starts
T=90:03:    Timer expires (3s), port shuts off
            → Phone stops LED flashing
            → Zero phantom power consumption
            
User unplugs phone, replugs 8 hours later:
T=8hr:      CC attach detected, port re-enabled
            → Charging resumes
```

---

## Power Consumption

**Microcontroller:**
- Active: ~2mA @ 8MHz
- Per-port enable circuitry: <1mA

**Total system standby:** <10mW when all ports off

**Per-port active:** <50mW (enable + sensing)

---

## Troubleshooting

### Port won't turn on when device plugged in

**Check:**
1. CC line connections (RC7, RB1, RC4, RC6, RC2, RC3, RA4, RA5)
2. CC pulldown resistors on USB connector (5.1kΩ to GND)
3. Enable pin connections (RB4, RB5, RA0, RA1)
4. Power to enable circuitry

### Port turns off too quickly / won't stay on

**Solutions:**
1. Increase `PERSIST_MS` (e.g., 5000 for 5 seconds)
2. Lower `LOW_CURRENT_MA` threshold (e.g., 5.0mA)
3. Check calibration factor against actual measurements

### Port turns off at wrong current level

**Calibration procedure:**
1. Connect known load (e.g., 10mA constant current)
2. Observe when port shuts off
3. Adjust `CALIBRATION_FACTOR`:
   - If shuts off at 14mA target 10mA: factor = 1.4
   - If shuts off at 7mA target 10mA: factor = 0.7
   - Formula: `new_factor = old_factor × (actual_mA / target_mA)`

### Port won't re-enable after charging complete

**This is correct behavior!**

Port will only re-enable when:
1. Device is physically unplugged (CC lines go HIGH)
2. Device is replugged (CC lines go LOW again)

This is the core feature preventing phantom power and LED flashing.

---

## Development Environment

**Required Software:**
- MPLAB X IDE (v6.0 or later)
- XC8 Compiler (v2.40 or later)
- MPLAB Code Configurator (MCC) for peripheral setup

**MCC Configuration:**
- System Clock: 8MHz internal oscillator
- ADC: 10-bit, FVR 4.096V reference
- GPIO: Configure all pins per pin table above
- No interrupts required (polled operation)

---

## Building and Programming

**Build:**
```
1. Open project in MPLAB X
2. Build → Clean and Build Main Project
3. Verify 0 errors, 0 warnings
```

**Program:**
```
1. Connect PICkit or ICD to ICSP header
2. Run → Program Device
3. Observe boot LED sequence
```

**Verification:**
```
1. Boot: All LEDs should flash twice
2. Plug device: Port LED turns on
3. Wait for charge: Port LED turns off after 3s below threshold
4. Unplug/replug: Port LED turns back on
```
---

## License

See main project LICENSE file.

---

## Author

Part of the USB Smart Charger hardware project.

For hardware documentation, schematics, and PCB files, see parent directory.
