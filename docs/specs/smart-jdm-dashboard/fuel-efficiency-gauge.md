# Fuel Efficiency Gauge Spec

## Status: Planned (web dashboard)

## Objective

Display real-time and trip-average fuel efficiency on the web dashboard. Shows instantaneous L/100km (or km/L) so the driver can adjust driving style for economy, alongside a trip average comparison bar.

---

## Data Source

### Instantaneous Fuel Efficiency

Computed on the frontend from telemetry:

```
instantL100 = fuelFlowLph / (speedKph / 100)
```

**Problem:** OBD-II fuel flow rate is not yet available. Until OBD is live, use a simulation model:

```js
// Simulated fuel flow (litres/hour) from RPM and speed
function estimateFuelFlow(rpm, speedKph) {
  const idleFlow = 0.8;                          // L/h at idle
  const loadFactor = rpm / 6000;                  // 0–1
  const flowLph = idleFlow + loadFactor * 6.5;    // 0.8–7.3 L/h range
  return flowLph;
}

function computeL100(fuelFlowLph, speedKph) {
  if (speedKph < 3) return 0;                     // avoid divide-by-near-zero
  return fuelFlowLph / (speedKph / 100);           // L/100km
}
```

When OBD-II is connected, replace with PID `0x5E` (Engine fuel rate) or compute from MAF sensor PID `0x10`.

### Trip Average

Accumulated over the trip using total fuel consumed and total distance:

```js
tripAvgL100 = (tripFuelUsedLitres / tripDistanceKm) * 100;
```

---

## Display Design

A compact gauge that fits in the dashboard info bar or as a secondary panel.

### Layout

```text
┌──────────────────────────────────┐
│  ⛽ FUEL EFFICIENCY              │
│                                  │
│      4.8                         │  ← Large instantaneous value
│    L/100km                       │
│                                  │
│  ████████░░░░  instant  4.8      │  ← Comparison bars
│  ██████████░░  average  6.2      │
│                                  │
│  Trip: 2.4 L used · 38.7 km     │  ← Trip fuel summary
└──────────────────────────────────┘
```

### Colors

| Value Range | Color | Meaning |
|-------------|-------|---------|
| < 5.0 L/100km | `#2dd4bf` (teal) | Excellent economy |
| 5.0–8.0 L/100km | `#fbbf24` (amber) | Normal |
| > 8.0 L/100km | `#fb7185` (rose) | Heavy consumption |

### Behavior

- Update every 500ms (smoothed with 5-sample rolling average to avoid jitter).
- At idle (speed < 3 km/h), show `--.-` instead of infinity.
- Bar widths scale relative to 15 L/100km max.

---

## WebSocket Payload

No new ESP32 fields required immediately. Frontend computes from existing fields:

| Field | Usage |
|-------|-------|
| `spd` | Speed for L/100km calculation |
| `rpm` | Fuel flow estimation (until OBD fuel rate) |
| `fuel` | Tank level for trip fuel-used delta |

### Future OBD Extension

When OBD-II integration is live, add:

```json
{ "fr": 4.2, ... }
```

| Field | Type | Description |
|-------|------|-------------|
| `fr` | float | Fuel rate in L/h from OBD PID 0x5E |

---

## File Changes

```
frontend/dashboard/
  app.js                        # Add fuel efficiency computation + display element
preview/features/
  fuel-efficiency.html          # Standalone preview
```

## Out Of Scope

- Fuel cost calculation (depends on fuel price lookup feature).
- CO₂ emissions estimate.
- MPG unit toggle (L/100km only for now).
