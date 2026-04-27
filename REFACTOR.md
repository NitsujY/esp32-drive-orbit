# Orb-Only Refactor: Independent OBD2 Architecture

## Overview

The project has been refactored to focus **exclusively on the Orb** as the single device in the car, eliminating the complex WiFi + ESP32 + Bluetooth relay through dash_35 (legacy). The Orb now reads OBD2 data directly via UART.

### Problem Solved
- **Before**: OBD2 → dash_35 (legacy) (gateway) ✗→ ESP-NOW broadcast ✗→ companion_orb (frequent data loss)
- **Now**: OBD2 → companion_orb (direct UART, reliable & simple)

---

## What Changed

### 1. Web Preview (✅ Complete)

**Location**: `preview/companion-orb/`

The interactive web preview now showcases the **independent orb** with:
- **Large speed display** (0–200 km/h) prominently overlaid on the orb canvas
- **RPM readout** (0–7000) with visual bar indicator
- **Pet tamagotchi features**: mood, XP, sleep, fuel level tracking
- **Drive modes**: Chill (blue) vs Sport (orange) theming
- **Real telemetry simulation**: Use sliders to simulate OBD2 data

To preview locally:
```bash
python3 -m http.server 8123 --directory preview/companion-orb
# Open http://localhost:8123
```

### 2. Firmware (✅ Complete)

**Location**: `src/companion_orb/`

#### New Files
- `app/elm327_obd2_client.h` — OBD2 client interface (simplifiedELM327 protocol)
- `app/elm327_obd2_client.cpp` — Implementation with UART communication

#### Updated Files
- `main.cpp` — Switched from ESP-NOW receiver to direct OBD2 polling
  - Removed: WiFi, ESP-NOW, channel hopping
  - Added: UART-based ELM327 OBD2 communication
  - Kept: Pet state, display rendering, telemetry processing

#### Key Architecture
```cpp
// New direct OBD2 flow:
OBD2 Adapter (ELM327)
    ↓ UART (38400 baud)
ESP32-C3 (pins 20/21)
    ↓ Polling
Orb Display + Pet Logic
```

### 3. Build Configuration (✅ Complete)

**File**: `platformio.ini`

- Changed `default_envs` from `dash_35` (legacy) → `companion_orb`
- Orb is now the default build target
- All companion_orb source files automatically included

---

## Hardware Wiring

### UART Connection (OBD2 to Orb)
For **ESP32-C3-DevKitM-1**:
- **Pin 20** → ELM327 RX
- **Pin 21** → ELM327 TX
- **GND** → ELM327 GND
- **3.3V** → ELM327 VCC

*Adjust pins in `elm327_obd2_client.cpp:begin()` if using different board.*

### Serial Monitor
Monitor on **Serial (USB)** for logs:
```
[BOOT] companion_orb starting (independent OBD2 mode)
[OBD2] Elm327Obd2Client starting
[OBD2] Init step 0: ATE0 (echo off)
[OBD2] Status: Connected | Speed: 65 km/h, RPM: 2500 | Fuel: 75%
```

---

## OBD2 Protocol (ELM327)

The client reads three core PIDs:
- **PID 0x0C** → Engine RPM (2 bytes: `(byte1 << 8 | byte2) / 4`)
- **PID 0x0D** → Vehicle Speed (1 byte, km/h)
- **PID 0x2F** → Fuel Tank Level Percentage (1 byte)

Query cycle: RPM → Speed → Fuel → repeat every ~600ms total.

---

## Testing Checklist

Before deployment:
- [ ] Verify ESP32-C3 builds successfully: `platformio run -e companion_orb`
- [ ] UART connection on pins 20 (RX) & 21 (TX) established
- [ ] ELM327 adapter powered at 3.3V
- [ ] OBD2 vehicle adapter connected to car
- [ ] Serial monitor shows successful OBD2 initialization (should see "Connected")
- [ ] Speed & RPM update every 200ms when car is running
- [ ] Pet tamagotchi responds to acceleration/speed patterns

---

## What's Deprecated

- **dash_35 (legacy)**: The headless gateway is no longer used. Kept in repo for reference.
- **ESP-NOW broadcast**: No longer needed for orb communication.
- **WiFi on orb**: Fully removed to simplify firmware and improve reliability.

---

## Future Enhancements

Potential next steps (optional):
- Add more OBD2 PIDs (coolant temp, throttle position, etc.)
- Implement fault code (DTC) reading
- Add WiFi web dashboard (on new device) for data logging
- Enhance pet behavior with more driving metrics

---

## Summary

✅ The Orb is now a **standalone, self-contained OBD2 device** with no external dependencies. The tamagotchi pet responds to real car telemetry, and the architecture is **simpler, faster, and more reliable** than the previous relay-based system.
