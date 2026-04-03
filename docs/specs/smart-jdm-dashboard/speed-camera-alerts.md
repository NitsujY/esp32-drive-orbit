# Speed Camera Alerts Spec

## Status: Planned

## Objective

Fetch fixed speed camera locations from an open database and flash a visual warning on the companion_orb when approaching one. The alert is distance-based — triggers when within a configurable radius.

## Data Source

### OpenStreetMap Overpass API

- Free, no API key required.
- Query: `[out:json];node["highway"="speed_camera"](around:{radius},{lat},{lon});out;`
- Returns lat/lon of all speed cameras within the radius.
- Radius: 5km around current position.

### Alternative: Static local database

- Download camera locations for your region as a JSON/CSV file.
- Store in SPIFFS on the ESP32 (16MB flash has room).
- No internet needed after initial download.
- Phase 2 enhancement.

## Alert Behavior

### Distance Thresholds

| Distance | Action |
|----------|--------|
| > 1000m | No alert |
| 500-1000m | Orb mood ring flashes amber briefly |
| 200-500m | Orb face shows alert eyes, mood ring pulses amber |
| < 200m | Orb face shows wide eyes, mood ring solid red, speed text flashes on orb |

### Alert Display on Orb

- Overrides the normal face temporarily (like the low-fuel face).
- Shows a simple "!" icon above the face.
- Current speed displayed prominently so the driver can check.
- Alert clears automatically when distance increases past 1000m.

### Alert on Dash

- Small camera icon appears in the top bar next to OBD status.
- Distance countdown: "CAM 350m".
- Disappears when out of range.

## Fetch Strategy

- Fetch camera locations every 5 minutes (or when position changes significantly).
- Phase 1: hardcoded lat/lon, fetch cameras around that point.
- Phase 2: support configurable per-location override stored in `wifi_config.h`.
- Cache the camera list — recalculate distances locally each loop cycle.

## Distance Calculation

- Use Haversine formula for lat/lon distance.
- Lightweight enough to run every render cycle on ESP32.
- Only calculate against cached camera list (typically < 50 points in 5km radius).

## Telemetry Extension

```cpp
uint16_t nearest_camera_m;    // distance to nearest camera in meters, 0xFFFF = none
```

- Added to `StatusTelemetryPayload` for orb display.

## Dependencies

- Wi-Fi hotspot connection.
- ArduinoJson.
- Location hardcoded in `wifi_config.h`; no GPS hardware required.

## Legal Note

- Speed camera alerts are legal in Japan and most countries.
- This is an informational display, not a radar detector.

## Out Of Scope

- Mobile/temporary camera detection
- Radar/laser detection hardware
- Speed limit display (requires map data)
- Audio alerts (no speaker on either board)
