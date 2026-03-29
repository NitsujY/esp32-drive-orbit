# Live Traffic Overlay Spec

## Status: Planned

## Objective

Fetch live traffic conditions for a configured commute route and reflect congestion state on the companion_orb face. The orb reacts emotionally to traffic — angry/frustrated in heavy congestion, happy when roads are clear.

## Data Source

### TomTom Traffic Flow API (free tier)

- 2,500 free requests/day — sufficient for 10-minute polling over a full day.
- Endpoint: `https://api.tomtom.com/traffic/services/4/flowSegmentData/absolute/10/json`
- Parameters: `point={lat},{lon}`, `key={api_key}`
- Response includes `currentSpeed`, `freeFlowSpeed`, `currentTravelTime`, `freeFlowTravelTime`.
- Congestion ratio: `currentSpeed / freeFlowSpeed` (1.0 = free flow, 0.3 = heavy congestion).

### Alternative: Google Roads API

- More accurate but requires billing account.
- Can be swapped in later with same congestion ratio output.

## Congestion Levels

| Ratio | Level | Orb Reaction |
|-------|-------|-------------|
| > 0.8 | Clear | Happy face, green mood ring tint |
| 0.5 - 0.8 | Moderate | Neutral face, amber mood ring tint |
| 0.3 - 0.5 | Heavy | Frustrated face (furrowed brows, flat mouth) |
| < 0.3 | Gridlock | Angry face (narrow eyes, frown), red mood ring pulse |

## Fetch Strategy

- Fetch every 10 minutes while Wi-Fi is connected.
- Use a configurable route point (lat/lon in `wifi_config.h`).
- Phase 2: multiple waypoints along a commute route.
- Cache last result — display cached data if fetch fails.

## Telemetry Extension

```cpp
uint8_t traffic_congestion_pct;  // 0 = gridlock, 100 = free flow, 255 = unknown
```

- Added to `StatusTelemetryPayload` and broadcast to orb via ESP-NOW.

## Orb Display Integration

- Traffic mood overlays on top of the existing speed-based mood system.
- Only active when parked or at low speed (< 20 kph) — at driving speed, the normal driving mood takes priority.
- During sleep mode, show traffic status as a colored ring segment (green/amber/red).

## Dependencies

- Wi-Fi hotspot connection (wifi-hotspot-weather.md).
- ArduinoJson for response parsing.
- TomTom API key stored in `wifi_config.h`.

## Out Of Scope

- Turn-by-turn navigation
- Route planning
- Real-time rerouting
