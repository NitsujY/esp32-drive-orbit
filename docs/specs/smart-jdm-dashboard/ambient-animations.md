# Ambient Animations Spec

## Status: Planned (web dashboard)

## Objective

Add two ambient animation layers to the web dashboard that respond to vehicle state and weather conditions. These are subtle background effects that enhance the visual experience without distracting the driver.

---

## 1. Idle Animation

### Trigger

- Speed is 0 km/h for more than 3 consecutive seconds (engine on, parked or at traffic light).
- Deactivates immediately when speed > 0.

### Visual Effect

A gentle **breathing particle field** behind the speed readout:

- 20–30 small luminous particles drift slowly upward from the bottom third of the screen.
- Particles have randomized size (1–3px), opacity (0.1–0.4), and drift speed.
- Overall brightness pulses on a slow 4-second sine cycle (opacity oscillates between 0.15 and 0.35).
- Color inherits the dashboard accent (teal `#2dd4bf` by default).
- Particles fade out over 0.6s when speed resumes.

### Performance

- Use CSS `will-change: transform, opacity` for GPU compositing.
- `requestAnimationFrame` loop — no `setInterval` jank.
- Cap at 30 particles max to keep mobile WebView smooth.

---

## 2. Weather Animation

### Trigger

- Activated when `wc` (WMO weather code) is present in telemetry.
- Animation changes based on weather condition.
- Runs continuously as a background layer behind all dashboard content.
- Intensity is deliberately subtle — never obscures speed readout.

### Weather Code Mapping

| WMO Code Range | Condition | Animation |
|----------------|-----------|-----------|
| 0 | Clear sky | No animation (or very faint star-like twinkle at night) |
| 1–3 | Partly cloudy | Slow-drifting translucent cloud wisps, left to right |
| 45, 48 | Fog | Low-opacity full-screen haze that gently pulses |
| 51–55 | Drizzle | Sparse thin streaks falling at slight angle, low opacity |
| 61–65 | Rain | Denser rain streaks, faster fall speed, subtle splash at bottom |
| 71–75 | Snow | Soft white dots drifting down with horizontal sway |
| 80–82 | Rain showers | Bursts of rain with varying intensity |
| 95–99 | Thunderstorm | Rain streaks + occasional brief screen-edge flash (white, 80ms) |

### Visual Rules

- All weather particles render at max 30% opacity — dashboard readability is sacred.
- Rain streaks: 1px wide, 12–24px tall, 60–80° angle, fall speed 200–400px/s.
- Snow: 2–4px dots, drift speed 30–60px/s, gentle horizontal sine wobble.
- Fog: radial gradient overlay, opacity pulses between 0.05 and 0.15 over 6s.
- Lightning flash: full-width top-edge glow, 80ms on → 200ms fade, max once every 8–15s (randomized).

### Canvas Layer

- Render on a dedicated `<canvas>` element positioned behind the dashboard UI (`z-index: -1`).
- Share the `requestAnimationFrame` loop with idle animation.
- Weather animation takes priority over idle animation — if both conditions are met, show weather only.

---

## Telemetry Requirements

Uses existing fields — no new telemetry needed:

| Field | Usage |
|-------|-------|
| `spd` | Idle detection (speed === 0) |
| `wc` | Weather code for animation selection |

---

## Transition Behavior

- When weather code changes, cross-fade between animations over 1 second.
- When entering idle state, fade in over 0.8s.
- When exiting idle state (speed > 0), fade out over 0.6s.

---

## File Changes

```
frontend/dashboard/
  app.js               # Import and mount animation canvas, subscribe to telemetry
preview/features/
  ambient-animations.html  # Standalone preview with weather code selector
```

## Out Of Scope

- Day/night auto-theme switching (separate feature).
- Companion orb ambient effects (orb deferred).
- Sound effects.
