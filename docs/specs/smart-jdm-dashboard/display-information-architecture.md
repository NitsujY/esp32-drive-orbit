# Display Information Architecture

## Status: Approved Design

## Principle

**Dash_35 = data + navigation.  Companion Orb = emotion + ambient.**

The driver should never need to look at both screens simultaneously. The orb is peripheral vision — felt, not read. The dash is intentional glance — quick lookup when needed.

---

## Dash_35 (480 × 320 landscape) — Primary Driver Display

### Always Visible (main screen)

| Element | Position | Notes |
| ------- | -------- | ----- |
| Speed | Center hero | Largest element on screen, always dominant |
| Speed unit | Right of speed | Baseline-aligned `KM/H`, never under the digits |
| RPM tachometer ribbon | Below speed | Full-width secondary motion layer, visually subordinate to speed |
| Drive status | Bottom-left status rail | Gear-first drivetrain context, may include live state |
| Energy field | Bottom-right status rail | Merge `fuel %` and `range km` into one compact module |
| Weather / ambient temp | Top-left metadata strip | Packed into one compact weather group |
| Rolling status | Top-center metadata strip | Rotating one-line status updates |
| Time | Top-right metadata strip | HH:MM, always right-aligned |

### Primary Motion Cluster

The dashboard should be read in this order:

1. **Speed first**: the speed digits are the hero and must remain the most legible element at a glance.
2. **RPM second**: the tach ribbon is the supporting performance layer, wide and expressive but visually lighter than the speed hero.
3. **Status third**: gear, energy, and similar slow-changing values belong to a lower rail, not in the main speed frame.

Rules:

- Speed should occupy the center of the composition with no competing numeric block nearby.
- The unit text should sit to the right of the speed or share its baseline zone, never directly underneath the digits where it risks overlap.
- The tach ribbon should sit immediately below the speed frame and read as a single performance band rather than as a separate card.
- Numeric RPM is optional in the default sport view; the ribbon itself should carry the motion emphasis.

### Gear Placement Recommendation

`GEAR` should **not** float inside the speed frame and should **not** compete with the top metadata strip.

Recommended placement:

- Use a compact lower-left badge attached to the bottom status rail.
- Keep it short (`P`, `D`, `D3`, `D4`) and visually similar to fuel/range modules.
- Treat it as drivetrain context, not as top-level hero information.

Recommended companion values for the same lower-left block:

- OBD state: `LIVE`, `SCAN`, `OFF`
- Shift context: `HOLD`, `SPORT`, `ECO` when relevant
- Camera-clear or assist-ready tokens only when needed

### Minimal Top Metadata Strip

The top bar should stay minimal and informational, not diagnostic-heavy.

Recommended default contents:

| Zone | Content | Notes |
| ---- | ------- | ----- |
| Left | Weather group | Packed text like `CLEAR / 24C` |
| Center | Rolling status | One short line that rotates every few seconds |
| Right | Time | e.g. `17:48` |

Guidelines:

- Prefer text tokens over persistent signal, battery, or decorative status icons.
- If a system state is healthy, omit it. Show badges only on exception or alert.
- When a camera, charging, or maintenance alert is active, it may temporarily replace part of the metadata strip.

Recommended rolling-status candidates:

1. `LIGHTS OFF AFTER SUNSET`
2. `COOLANT 82C`
3. `BATTERY 13.8V`
4. `TRIP 12.4KM`
5. `CAM 350M`
6. `WIFI LINKED`

Rules for the rolling status:

- Show only one message at a time.
- Rotate every 2 to 4 seconds.
- Interrupt rotation immediately for critical alerts.
- Keep each token under about 14 characters when possible.

### Energy Field Recommendation

`FUEL` and `RANGE` can be merged visually into a single lower-right `ENERGY` field.

Recommended format:

- Primary text: `70% / 410km`
- One shared fill bar driven by fuel percentage
- Range treated as the textual estimate, not as a second competing gauge

This keeps the lower rail cleaner while still showing both tank state and expected travel distance.

### Reminder Zone Recommendation

Use the top-center status lane as the default reminder zone and escalate to the full-width top alert banner only when the reminder is urgent.

Recommended behavior for the headlight reminder:

- Trigger condition: after sunset or before sunrise, vehicle moving, headlights still OFF.
- Soft reminder: replace the rolling status with `LIGHTS OFF AFTER SUNSET` for 4 to 6 seconds.
- Escalation: if the condition persists for about 30 seconds, promote it to the full-width top banner.
- Clear condition: headlights turn ON, vehicle stops, or the car returns to daylight.

Current Toyota Sienta probe result:

- Current best OBD source for `headlights_on`: header `7C0`, Mode `21`, PID `21C4`.
- Observed compact values: `7C80361C40C` with headlights OFF and `7C80361C40B` with headlights ON.
- The byte-level working assumption for later firmware mapping is `0x0C = OFF` and `0x0B = ON`.
- This signal still requires repeated ignition-cycle and day/night validation before it is treated as fully locked.

This keeps reminders close to the driver's primary glance path without polluting the lower drivetrain rail or competing with the speed hero.

### Top-Bar Alert Banners (highest priority, full width)

Shown one at a time, cycling if multiple are active:

| Feature | Banner Text |
| ------- | ----------- |
| Cold start | `LET IT WARM — coolant 48°C` |
| Maintenance due | `OIL DUE 320 km` |
| Speed camera | `CAM 350m` (small badge, not full banner) |
| Weather advisory | `⚠ WET ROADS — take care` |
| Idle over budget | `IDLE 6:12 — consider engine off` |
| Charging fault | `CHARGING FAULT` |

### Detail View (touch toggle — main cluster tap)

All secondary data lives here. Swaps in to replace the speed cluster:

| Row | Left card | Right card |
| --- | --------- | ---------- |
| 1 | Trip Score + Grade | Harsh event counts |
| 2 | Instant km/L | Trip distance |
| 3 | AQI badge | UV Index |
| 4 | Battery SoC | Coolant temp |
| 5 | Fuel price signal | Fuel station name |
| 6 | Idle time | CO₂ estimate |

### What the Dash NEVER Shows

- Weather icon / temperature — orb handles this
- Emotion or mood label — orb handles this
- Post-trip celebration animation — orb handles this
- Time in sleep mode — orb handles this

---

## Companion Orb (240 × 240 round) — Emotional Ambient Display

### Normal Driving Face

| Zone | Content | Notes |
| ---- | ------- | ----- |
| Top | Weather icon + temp | e.g. `☀ 24°C`, hidden in Sport mode |
| Center | Face character | Driven by emotion priority system |
| Below face | Trip score `87` | Updated every 30s |
| Below score | Grade label `smooth` / emotion name | Short, 1–2 words |
| Bottom | Advisory text | `WET ROADS`, `slow down`, etc. |

### Emotion Priority Stack (highest wins)

| Priority | Trigger | Face | Ring |
| -------- | ------- | ---- | ---- |
| 1 | Speed camera < 200m | `! !` | Red, fast pulse |
| 2 | Harsh brake just now | `≧﹏≦` | Amber flash |
| 3 | Harsh accel just now | `○o○` | White flash |
| 4 | Cold start over-rev | `ˊ_ˋ` | Slow amber |
| 5 | High-RPM cruising | `⊙_⊙` | Fast amber |
| 6 | Long idle > 5 min | `¬_¬` | Slow white |
| 7 | Good streak (5 min clean) | `◠‿◠` | Slow green |
| 8 | Default speed/RPM mood | `◡‿◡` | Speed-based |

### Sleep Mode (parked > 20s)

```text
  05:41
[closed eyes]
  ☀ 24°C
```

- Large clock, closed eyes below, weather temp at bottom.
- Mood ring dims to a very slow gentle pulse.
- No OBD data shown — orb is a clock while parked.

### Post-Trip (engine off → score computed)

| Score | Orb Reaction | Ring |
| ----- | ------------ | ---- |
| ≥ 90 | `★‿★` star eyes | Rainbow cycle 5s |
| 75–89 | `◠‿◠` wide smile | Green pulse 3s |
| 60–74 | `◡‿◡` neutral | White pulse 2s |
| < 60 | `◞_◟` droopy | Dim amber 2s |

### What the Orb NEVER Shows

- Exact speed number — that's on the dash
- RPM number — that's on the dash
- Fuel percentage or range — too much text for round screen
- Maintenance reminder banners — too verbose for orb
- AQI number — shows haze tint only (no text in normal mode)
- Fuel price — too much text for orb

---

## Ambient Overlays (applied on top of the existing face)

These do not replace the face — they layer over or under it:

| Feature | Orb Overlay | Trigger |
| ------- | ----------- | ------- |
| AQI > 100 | Semi-transparent haze tint on glass | Continuous while bad |
| Rain / storm | Orb eyes droop slightly (eyebrow modifier) | Weather code check |
| Clear sunny | Slightly wider smile baseline | Weather code check |
| Night mode | Ring shifts to cool purple | After sunset |

---

## Design Rules (enforce these in all future feature specs)

1. **Text on orb ≤ 8 characters per line.** The 240px display cannot fit more.
2. **Orb never shows raw numbers unless they fit in 3 characters** (e.g. `87`, `24°`, `S`).
3. **Dash detail view is the home for all numeric precision** — never crowd the main screen with it.
4. **Any alert that requires reading** (e.g. `OIL DUE 320km`) goes on dash only.
5. **Any state that is felt not read** (mood, vibe, ambient) goes on orb only.
6. **Sport mode on orb** hides weather and shows only face + gear because the driver is focused.
7. **Speed and tach are the primary dashboard hierarchy** — slow-changing status data must defer to them visually.
8. **Gear belongs to the lower status rail by default** unless a transient shift-related alert needs special emphasis.
