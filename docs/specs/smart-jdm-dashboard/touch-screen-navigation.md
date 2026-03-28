# Touch Screen Navigation Spec

## Status: Planned

## Objective

Replace the BOOT button (GPIO 0) with the Waveshare ESP32-Touch-LCD-3.5 built-in capacitive touch panel for screen navigation on the dash_35.

## Hardware

- The Waveshare board includes a GT911 capacitive touch controller.
- Connected via I2C (same bus as the TCA9554 IO expander, pins 21/22).
- I2C address: typically 0x5D or 0x14 (GT911 has two possible addresses).
- Touch resolution matches the display: 480x320 pixels.
- Interrupt pin: check Waveshare schematic (likely GPIO 4 or routed through the IO expander).

## Touch Zones

The 480x320 landscape display is divided into tap zones:

```
+--------------------------------------------------+
|  [vehicle name]              [status]   [mode]   |  <- top bar (tap: cycle info)
|--------------------------------------------------|
|                                                  |
|              [speed + RPM arc cluster]           |  <- center (tap: toggle detail)
|                                                  |
|--------------------------------------------------|
|  [mode]    [gear]    [range]    [fuel]           |  <- bottom bar (tap: cycle cards)
+--------------------------------------------------+
```

### Tap Actions

| Zone | Tap | Long Press |
|------|-----|------------|
| Center cluster | Toggle detail view (replaces BOOT button) | — |
| Top bar | Cycle OBD status detail | — |
| Bottom bar | Cycle bottom info (mode/gear/range/fuel/trip) | Reset trip metrics |

### Swipe Actions (future)

- Swipe left/right: cycle between dashboard screens (main, trip, health)
- Swipe down: pull down notification shade (future alerts)

## Implementation Approach

### Phase 1: Basic Tap (replaces BOOT button)

1. Add GT911 driver initialization in `DashboardDisplay::begin()`.
2. Poll touch in `DashboardDisplay::pollButton()` — replace `digitalRead(GPIO 0)` with touch read.
3. Map touch coordinates to zones.
4. Center zone tap toggles `detail_view_` (same as current BOOT button behavior).

### Phase 2: Zone Navigation

1. Add zone detection based on touch Y coordinate:
   - Y < 52: top bar zone
   - Y 52-252: center cluster zone
   - Y > 252: bottom bar zone
2. Implement tap handlers per zone.
3. Add debounce (200ms minimum between taps).

### Phase 3: Swipe (deferred)

1. Track touch-down and touch-up positions.
2. Calculate delta X/Y to detect swipe direction.
3. Minimum swipe distance: 60px.

## Library Options

1. **Direct GT911 I2C** — minimal driver, read touch points from registers. Lightweight, no dependencies.
2. **Waveshare example driver** — reference code from Waveshare wiki for this specific board.
3. **Arduino GT911 library** — community library, may need adaptation.

Recommended: option 1 (direct I2C) to keep dependencies minimal.

## GT911 Register Map (key registers)

| Register | Size | Description |
|----------|------|-------------|
| 0x8047   | 1    | Number of touch points (0-5) |
| 0x8048   | 4    | Touch point 1: X low, X high, Y low, Y high |
| 0x814E   | 1    | Buffer status (bit 7 = data ready, bits 3:0 = point count) |

## Dependencies

- I2C bus already initialized in `DashboardDisplay::begin()` (Wire.begin(21, 22)).
- No additional lib_deps required if using direct I2C register reads.

## Migration Path

- Phase 1 can coexist with the BOOT button — both inputs work.
- Once touch is validated, the BOOT button check can be removed.
- The `pollButton()` method signature stays the same; only the implementation changes.

## Out Of Scope

- Multi-touch gestures (pinch, rotate)
- Touch on the companion_orb (no touch controller on the GC9A01 module)
- Haptic feedback
