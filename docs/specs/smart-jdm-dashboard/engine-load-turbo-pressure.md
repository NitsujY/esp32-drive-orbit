# Engine Load & Turbo Pressure Spec

## Status: Planned (web dashboard + OBD-II dependency)

## Objective

Display real-time engine load percentage and turbo/intake manifold pressure on the web dashboard. These gauges give the driver visibility into powertrain effort — especially relevant for turbocharged JDM vehicles where boost monitoring is a core enthusiast feature.

---

## Data Source

### OBD-II PIDs

| PID | Name | Formula | Unit | Notes |
|-----|------|---------|------|-------|
| `0x04` | Calculated Engine Load | `A * 100 / 255` | % | 0–100%, available on all OBD-II vehicles |
| `0x0B` | Intake Manifold Absolute Pressure | `A` | kPa | Atmospheric ≈ 101 kPa; turbo boost = value − 101 |
| `0x33` | Barometric Pressure | `A` | kPa | For altitude-corrected boost: boost = MAP − baro |

**Engine Load** is universally available. **Turbo pressure** requires PID `0x0B` (MAP sensor), which is standard on all fuel-injected vehicles. Actual boost = MAP − barometric pressure.

### Simulation Model (pre-OBD)

Until OBD-II Bluetooth is connected, simulate from telemetry:

```js
function simulateEngineLoad(rpm, speedKph, speedIncreasing) {
  const rpmLoad = rpm / 6000;                // RPM contribution (0–1)
  const accelBonus = speedIncreasing ? 0.15 : 0;
  return Math.min(100, Math.round((rpmLoad * 70 + accelBonus * 100)));
}

function simulateBoostKpa(rpm, engineLoad) {
  if (engineLoad < 30) return 0;             // no boost at light load
  const boostFrac = (engineLoad - 30) / 70;  // 0–1 above 30% load
  const rpmFactor = Math.min(rpm / 4000, 1); // boost builds with RPM
  return Math.round(boostFrac * rpmFactor * 140); // max ~140 kPa (≈20 psi)
}
```

---

## Display Design

### Portrait — Dual Radial Gauges

Two side-by-side circular gauges centered vertically:

```text
┌──────────────────────────────────┐
│         ENGINE TELEMETRY         │
│                                  │
│    ┌─────────┐  ┌─────────┐     │
│    │  72%    │  │  0.8    │     │
│    │  load   │  │  bar    │     │
│    │ (arc)   │  │ (arc)   │     │
│    └─────────┘  └─────────┘     │
│                                  │
│  RPM 2400 · Speed 62 km/h       │
│  MAP 142 kPa · Baro 101 kPa     │
└──────────────────────────────────┘
```

### Landscape — Horizontal Bar + Boost Needle

Compact layout for 480 × 320:

```text
┌─────────────────────────────────────────┐
│  LOAD ██████████████░░░░░░  72%         │
│  BOOST ─────────────[needle]──── 0.8bar │
│  RPM 2400 · 62 km/h · MAP 142 kPa      │
└─────────────────────────────────────────┘
```

### Colors

| Value | Engine Load Color | Boost Color |
|-------|------------------|-------------|
| Low (0–40%) | `#2dd4bf` (teal) | `#555` (no boost) |
| Medium (40–75%) | `#fbbf24` (amber) | `#2dd4bf` (teal) |
| High (75–100%) | `#fb7185` (rose) | `#fbbf24` (amber) |
| Overboost (>1.4 bar) | — | `#fb7185` (rose, flash) |

### Boost Units

- Display in **bar** (1 bar = 100 kPa) for international/JDM convention.
- Formula: `boostBar = (mapKpa - baroKpa) / 100`
- Negative values (vacuum) shown as `VAC` with a different color zone.

### Behavior

- Update every 100ms matching telemetry cadence.
- Engine load arc: 270° sweep from 7 o'clock to 5 o'clock, same as RPM gauge convention.
- Boost gauge: 180° sweep (bottom half) from −1.0 bar vacuum to +1.5 bar boost.
- Overboost alert: if boost > 1.4 bar, the gauge border pulses rose for 2 seconds.

---

## WebSocket Payload Extension

New fields when OBD-II is connected:

```json
{ "el": 72, "map": 142, "baro": 101, ... }
```

| Field | Type | Description |
|-------|------|-------------|
| `el` | int | Engine load % (0–100) from PID 0x04 |
| `map` | int | Intake manifold pressure kPa from PID 0x0B |
| `baro` | int | Barometric pressure kPa from PID 0x33 (optional, default 101) |

### Schema Update

Add to `schema.json`:

```json
{
  "el": { "type": "integer", "min": 0, "max": 100, "unit": "%", "description": "Engine load" },
  "map": { "type": "integer", "min": 0, "max": 255, "unit": "kPa", "description": "Intake manifold pressure" },
  "baro": { "type": "integer", "min": 0, "max": 255, "unit": "kPa", "description": "Barometric pressure" }
}
```

### C++ Struct Extension

Add to `CarTelemetry`:

```cpp
uint8_t engine_load_pct = 0;        // PID 0x04, 0–100
uint8_t intake_map_kpa = 101;       // PID 0x0B, atmospheric default
uint8_t barometric_kpa = 101;       // PID 0x33, default sea level
```

---

## File Changes

```
include/
  car_telemetry.h                   # Add engine_load, MAP, baro fields
frontend/dashboard/
  schema.json                       # Add el, map, baro definitions
  app.js                            # Add engine/boost gauge panel
preview/features/
  engine-boost.html                 # Standalone preview
```

## Out Of Scope

- Knock sensor monitoring (PID not universally available).
- Wastegate duty cycle.
- Exhaust gas temperature (EGT) — requires aftermarket sensor.
- Boost controller integration.
