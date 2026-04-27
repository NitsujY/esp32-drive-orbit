# Trip Summary Overlay Spec

## Status: Implemented (web dashboard)

## Objective

A swipe-up overlay accessible from the info bar at the bottom of the Vertical Stack dashboard. Reveals a full-screen trip summary sheet without leaving the main view. Provides at-a-glance post-drive insight: distance, duration, average speed, eco score, and event counts.

---

## Trigger

| Gesture | Action |
| ------- | ------ |
| Swipe up from info bar | Open trip summary overlay |
| Swipe down on overlay | Dismiss |
| Tap outside overlay | Dismiss |
| Long-press trip odometer text | Open overlay (fallback for accessibility) |

The info bar (`gear · fuel · trip km`) acts as the drag handle — the user swipes upward from this element to reveal the sheet.

---

## Visual Design

The overlay is a **bottom sheet** that slides up over the main dashboard. The dashboard remains visible and dimmed behind it.

### Orientation Behavior

#### Landscape (primary — iPhone in car mount)

- Overlay covers approximately 72% to 78% of screen height.
- The sheet rises from the bottom and the body reorganizes into **two columns**.
- Left column: four key stats (`duration`, `distance`, `avg speed`, `max speed`).
- Right column: score, progress bars, and event rows.
- Safe area insets respected for Dynamic Island and home indicator.

#### Portrait (fallback)

- Overlay covers approximately 85% of screen height.
- Content stays stacked vertically: stats first, then score, then event rows.
- Feels like a full trip card pulled over the live dashboard.

### Sheet Anatomy

```text
┌────────────────────────────────────────┐
│       ── (drag handle)                 │
│  TRIP SUMMARY           [Reset]        │  ← header
│                                        │
│  ┌──────────────┬──────────────┐       │
│  │  0:47         │ 12.4 km     │       │  ← stat row
│  │  duration     │ distance    │       │
│  └──────────────┴──────────────┘       │
│  ┌──────────────┬──────────────┐       │
│  │  38 km/h     │ 72 km/h      │       │
│  │  avg speed   │ max speed    │       │
│  └──────────────┴──────────────┘       │
│                                        │
│  ── Score ─────────────────────        │
│  82 / A  Solid Driver                  │  ← score badge
│  ████████████░░░░░  smoothness         │
│  ████████████████░  speed consistency  │
│                                        │
│  ── Events ────────────────────        │
│  ⚡ 2 harsh accelerations              │
│  🛑 1 harsh braking                    │
│  ✓ 0 over-speed events                │
│                                        │
│                                        │
└────────────────────────────────────────┘
```

### Colors (inherits from vertical-stack dark theme)

| Element | Color |
| ------- | ----- |
| Sheet background | `#131313` |
| Drag handle | `rgba(255,255,255,0.15)` |
| Stat value | `#fafafa` |
| Stat label | `#555` |
| Score accent (A/S) | `#2dd4bf` (teal) |
| Score accent (B) | `#fbbf24` (amber) |
| Score accent (C/D) | `#fb7185` (rose) |
| Progress bar fill | matches score accent |
| Event row icon | per event color |
| Reset button | `rgba(255,255,255,0.08)` border, muted text |

---

## Data Model

All values are computed from live telemetry on the frontend (web/iOS) and from NVS on the ESP32. See also: `persistent-trip-data.md` and `driver-behavior-scoring.md`.

### Displayed Fields

| Field | Source |
| ----- | ------ |
| Duration | `(now - tripStartTimestamp)` → format as `H:MM` or `MM:SS` |
| Distance km | Accumulated from `speedKph` ticks: `speed / 36000` per 100ms |
| Avg speed | `totalDistanceKm / durationHours` |
| Max speed | Rolling max of all `speedKph` values |
| Smoothness score | 0–100 per `driver-behavior-scoring.md` |
| Speed consistency | 0–100 sub-score |
| Lifetime grade | S/A/B/C/D from rolling 20-trip average |
| Harsh accel count | Events where RPM delta > threshold in < 1s |
| Harsh brake count | Events where speed drop > threshold in < 1s |
| Over-speed count | Events where speed > configurable soft limit |
| Fuel used (est.) | `fuelAtTripStart - fuelCurrent` in % → convert to litres via tank capacity |

### Frontend State

```js
const tripStats = {
  startTimestamp: Date.now(),    // set when new trip begins
  maxSpeedKph: 0,
  harshAccelCount: 0,
  harshBrakeCount: 0,
  overSpeedCount: 0,
  prevSpeedKph: 0,               // for delta detection
  prevRpm: 0,                    // for delta detection
  fuelAtStart: null,             // set on first telemetry tick
};
```

### Derived at Render Time

```js
const durationMs = Date.now() - tripStats.startTimestamp;
const avgSpeedKph = tripDistanceKm / (durationMs / 3600000);
const smoothnessScore = computeSmoothnessScore(tripStats);
```

---

## Device Notes

The trip summary overlay runs on the **iPhone (Capacitor iOS app)**, not on the ESP32 TFT.

- The `dash_35` (legacy) ESP32 is a headless telemetry gateway — it has no local UI rendering.
- All trip sheet rendering, gestures, and animations are handled by the web frontend on the iPhone.
- In landscape (primary orientation), the sheet body reorganizes into two columns for readability.
- In portrait (fallback), the content stays stacked vertically.

---

## Trip Reset Behavior

- **Reset button** in the overlay: clears all per-trip counters (`tripDistanceKm`, `harshAccelCount`, etc.), resets `startTimestamp`, dismisses overlay
- **Auto-reset**: When OBD transitions from `OFF → LIVE` (engine start detected), trip metrics auto-reset. User is NOT prompted — the assumption is a new drive is starting.
- **Confirmation**: No confirmation dialog. Reset is single-tap. Undo is not provided (aligns with driver-safety principle of minimal interruption).

---

## Web Preview

A standalone preview is available at:  
`/features/trip-summary.html`

Demonstrates both portrait and landscape variants of the overlay animation, simulated stats, and score rendering without a live WebSocket connection.

---

## Out of Scope (this spec)

- Multi-trip history log (requires persistent storage beyond this scope)
- Cloud sync or sharing
- Fuel price per trip calculation
- Navigation / route overlay
