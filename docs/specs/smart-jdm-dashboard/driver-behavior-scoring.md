# Driver Behavior Scoring Spec

## Status: Implemented (web dashboard — trip-summary-overlay)

> **Design note:** The companion_orb emotion system originally in this spec has been removed.
> The dash_35 (legacy) is now a headless telemetry gateway; the companion_orb is deferred.
> Scoring is computed **on the web dashboard frontend** and displayed via the trip-summary-overlay sheet.
> ESP32-side NVS persistence of `last_trip_score` remains a future firmware enhancement (see `persistent-trip-data.md`).


## Scoring Model

### Input Signals (from live OBD + app-state)

| Signal | How Captured |
|--------|-------------|
| Harsh acceleration | RPM delta > threshold in < 1s |
| Harsh braking | Speed drop > threshold in < 1s (inferred from speed change rate) |
| High-RPM cruising | RPM sustained above 3500 for > 10s while speed is stable |
| Over-speed (relative) | Speed above profile's soft limit (e.g. 120 km/h) |
| Smooth speed-hold | Speed variance < 5 km/h over a 60s window |
| Cold-start over-rev | RPM > 2500 when coolant temp < 60°C |
| Long idle | Speed == 0 and RPM > 600 for > 3 minutes |
| Clean stop | Speed reaches 0 with no harsh-braking event in the final 10s |

### Per-Trip Score (0–100)

Start at 100. Apply penalties:

| Event | Penalty |
|-------|---------|
| Harsh acceleration | −3 per event |
| Harsh braking | −4 per event |
| High-RPM cruising (each 10s window) | −1 |
| Over-speed | −5 per event |
| Cold-start over-rev | −5 (once per trip) |
| Long idle > 3 min | −2 |

Apply bonuses (once per trip, capped at +10 total):

| Achievement | Bonus |
|-------------|-------|
| Zero harsh events | +5 |
| Smooth speed-hold maintained > 50% of trip | +3 |
| Trip distance > 10 km with score ≥ 85 | +2 |

Score is clamped to `0..100`. Stored in `TripMetrics::smoothness_score`.

### Lifetime Driver Grade

Computed from the rolling average of the last 20 trip scores:

| Average Score | Grade |
|---------------|-------|
| 90–100 | S — Silky Smooth |
| 75–89 | A — Solid Driver |
| 60–74 | B — Room to Improve |
| 40–59 | C — Rough Edges |
| < 40 | D — Take it Easy |

---

## Trip Score Display

Score and grade are shown in the **trip-summary-overlay** bottom sheet on the web dashboard.
Fields exposed: numeric score (0–100), letter grade, smoothness bar, speed consistency bar, event counts.

---

## Implementation Approach

### Phase 1: Web dashboard (complete)

Score formula runs in `frontend/dashboard/app.js`.
Event counters (`harshAccelCount`, `harshBrakeCount`, `overSpeedCount`) are accumulated per 100ms telemetry tick.
Score and grade are computed on demand when the trip sheet is opened.

### Phase 2: ESP32 NVS persistence (future)

Persist `last_trip_score` and event counts in NVS via `app::storage` so trip history survives power cycles.
See `persistent-trip-data.md`.

---

## Dependencies

- `persistent-trip-data.md` — NVS storage for score history (future).
- No additional libraries required.

## Out Of Scope

- companion_orb emotion reactions (orb deferred)
- ML-based driving style classification
- Leaderboard / cloud score sync
- Insurance-grade telematics (g-sensor required)
