# Driver Behavior Scoring Spec

## Status: Planned

## Objective

Use trip data already collected by the `dash_35` to compute a per-trip and lifetime driver score, then have the `companion_orb` reflect that score through expressive emotions — going beyond speed and RPM to reward overall smooth, courteous driving.

---

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

## Trip Score Display on Dash

- Show score at trip end in the detail view: `TRIP SCORE  87 / A`.
- Persist the last trip score in NVS (`last_trip_score`).
- Show grade in the top bar when parked (replaces OBD status briefly for 5s).

---

## Orb Emotion System

The orb already uses a speed/RPM face. This spec extends that to a richer emotion layer driven by driving behavior.

### Emotion Priority (highest wins)

| Priority | Trigger | Face |
|----------|---------|------|
| 1 | Speed camera proximity < 200m | ⚠ Wide eyes, red ring |
| 2 | Harsh braking just occurred (< 3s ago) | 😬 Wincing — eyebrows up, eyes squeezed |
| 3 | Harsh acceleration just occurred (< 3s ago) | 😲 Shocked — wide round eyes |
| 4 | Cold-start over-rev | 😟 Worried — angled brows, frown |
| 5 | High-RPM cruising | 😰 Anxious — side-glancing eyes |
| 6 | Long idle > 3 min | 😑 Bored — flat line eyes, slow blink |
| 7 | Trip score ≥ 90 at 5-min mark | 😊 Happy — squinting happy eyes |
| 8 | Default speed-based mood (existing) | — current system |

### Post-Trip Celebration

When OBD disconnects (engine off) and the trip score is computed:

| Score | Orb Reaction | Duration |
|-------|-------------|----------|
| ≥ 90 | 🎉 Star-eyes + mood ring cycles rainbow | 5s |
| 75–89 | 😊 Wide smile + mood ring pulses green | 3s |
| 60–74 | 🙂 Neutral smile + mood ring white pulse | 2s |
| < 60 | 😔 Droopy eyes + mood ring dims amber | 2s |

After the reaction, the orb transitions to sleep mode (parked face / clock).

### Emotion State Machine

```
Driving:
  default_mood ← speed/RPM (existing)
  ON harsh_brake    → WINCE for 3s, then return
  ON harsh_accel    → SHOCKED for 3s, then return
  ON cold_overrev   → WORRIED until coolant > 60°C
  ON high_rpm_cruise → ANXIOUS while condition holds
  ON long_idle      → BORED while condition holds
  ON good_streak (5min no events, score ≥ 88)
                   → HAPPY for 10s, then return

Parked:
  score_computed → POST_TRIP_REACTION for N seconds
  then → SLEEP
```

### Emotion to Face Mapping

| Emotion | Eye Shape | Eyebrow | Mouth | Mood Ring |
|---------|-----------|---------|-------|-----------|
| WINCE | Squeeze lines | Raised center | Thin line | Pulses red once |
| SHOCKED | Large circles | Raised flat | Small "O" | Flash white |
| WORRIED | Half-closed | Angled inward | Slight frown | Slow amber |
| ANXIOUS | Side-pupils | Flat raised | Neutral | Fast amber pulse |
| BORED | Flat lines | Flat | Straight line | Very slow white |
| HAPPY | Curved closed | Arched up | Wide U | Slow green pulse |
| CELEBRATING | Stars / spirals | Up | Big smile | Rainbow cycle |
| DEJECTED | Droopy half | Down angle | Down curve | Dim amber |

All faces are drawn as pixel-art primitives on the 240×240 GC9A01 (no bitmaps needed — same approach as existing face renderer).

---

## Telemetry Extension

Add to `StatusTelemetryPayload`:

```cpp
uint8_t trip_score;           // 0–100, current trip score, updated every 30s
uint8_t orb_emotion_hint;     // enum: NORMAL, WINCE, SHOCKED, WORRIED, ANXIOUS, BORED, HAPPY, CELEBRATING, DEJECTED
```

- `trip_score` lets the orb display the score independently.
- `orb_emotion_hint` is set by the dash_35 event detector; orb trusts it rather than re-deriving from raw telemetry.

```cpp
enum OrbEmotion : uint8_t {
  ORB_EMO_NORMAL      = 0,
  ORB_EMO_WINCE       = 1,
  ORB_EMO_SHOCKED     = 2,
  ORB_EMO_WORRIED     = 3,
  ORB_EMO_ANXIOUS     = 4,
  ORB_EMO_BORED       = 5,
  ORB_EMO_HAPPY       = 6,
  ORB_EMO_CELEBRATING = 7,
  ORB_EMO_DEJECTED    = 8,
};
```

---

## Implementation Approach

### Phase 1: Scoring only (dash_35)

1. Add event counters to `AppState` (already partially present via `harsh_acceleration_count` etc.).
2. Implement score formula in `DashboardState` at trip end.
3. Persist `last_trip_score` via `app::storage`.
4. Display score in detail view.

### Phase 2: Basic orb emotions

1. Add `orb_emotion_hint` to telemetry payload.
2. Implement emotion state machine in `DashboardState` (sets the hint field).
3. Implement emotion face renderer in `OrbDisplay` (draw the face variants).

### Phase 3: Post-trip celebration

1. Detect OBD disconnect → compute score → set celebration emotion.
2. Play celebration sequence then transition to sleep on the orb.

---

## Dependencies

- `persistent-trip-data.md` — NVS storage for score history.
- `wifi-hotspot-weather.md` — telemetry payload struct (`StatusTelemetryPayload`).
- No additional libraries required.

## Out Of Scope

- ML-based driving style classification
- Leaderboard / cloud score sync
- Insurance-grade telematics (g-sensor required)
- Aggressive overtaking detection (requires relative speed data)
