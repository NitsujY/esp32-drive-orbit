# Companion Orb Spec

## Status: Being Refactored

## Document Control

- Date: 2026-04-19
- Project: ESP32 Drive Orbit
- Scope: `companion_orb` firmware — round display personality unit

---

## Objective

The round display unit was originally conceptualized strictly as an ambient, glanceable companion communicating driving state and mood through animation and face expressions.

**Recent Update:** The project direction is splitting this circle display into *two* distinct view modes (or firmwares):
1. **The Companion Face View**: The original pet face that reacts emotionally.
2. **The Data Gauge View**: A minimalist data overlay focused directly on speed, load, and gas without the face.

This specification describes the overarching hardware but will now differentiate between the two display modes.

---

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-C3 DevKitM-1 (RISC-V, single-core 160 MHz) |
| Display | GC9A01 — 240 × 240 px round TFT (IPS, 65K colour) |
| Interface | SPI — SCLK:6 MOSI:7 CS:10 DC:2 RST:1 BL:3 |
| Backlight | PWM via `analogWrite` on pin 3 |
| Communication | ESP-NOW (receiver only, broadcasts from `dash_35` (legacy)) |
| Framework | Arduino + PlatformIO `env:companion_orb` |
| Display library | Arduino_GFX v1.6 |

---

## System Role

```
dash_35 (legacy)  ──ESP-NOW broadcast──►  companion_orb
           DashboardTelemetry           │
           (binary framed, ~10 Hz)      │
                                 TelemetryStreamParser
                                        │
                                  OrbDisplay::render()
```

`companion_orb` is receive-only. It never sends data back to `dash_35` (legacy).

---

## View Mode 1: Data Gauge Mode (New Default)

This mode operates as a direct data visualization tool in place of the pet face. The center space is reclaimed for telemetry numbers.

### Display Layout

- **Center Target:** Large digital Speed numerical readout (`km/h`).
- **Inner Rim (Gas):** A dashed or segmented arc originating from the bottom, calculating a percentage based on fuel.
- **Inner Rim (Load):** An arc that grows and changes colors dynamically from orange to red based on engine load.
- *Notes:* This view eliminates the traditional RPM vs Boost arcs (pink/blue) and replaces the entire pet face structure to focus entirely on the central speed digit and peripheral support arcs.

---

## View Mode 2: Companion Face Mode (Legacy/Alternative)

This view retains the original emotional companion focus.

### Display Layout

The 240 × 240 circle is divided into three vertical zones:

```
┌──────────────────────────┐
│  ┌──────────────────┐    │   ← Mood ring (outer arc, r 113–119)
│  │     Face area    │    │   ← Face (eyes + mouth, centred ~(120, 100))
│  │                  │    │
│  │  Gear / Score    │    │   ← Bottom info zone (~y 142–208)
│  └──────────────────┘    │
└──────────────────────────┘
```

The face expresses 6 moods depending on RPM and speed:
| `CompanionMood` | Expression |
|-----------------|------------|
| `Idle` (0) | Neutral horizontal line mouth, neutral eyes |
| `Warm` (1) | Gentle sine-curve smile |
| `Alert` (2) | Open-circle mouth, periodic blink every ~1.2 s |
| `Happy` (3) | Wide double-pixel smile arc, squinted arc eyes |
| `Sad` (4) | Drooped angled eyebrows, frown arc |
| `Excited` (5) | Cross/star eyes, open circle mouth with radial fill |

---

*(Rest of the system architectural choices remain the same: Light/Dark mode, ESP-NOW binary formats, etc.)*
