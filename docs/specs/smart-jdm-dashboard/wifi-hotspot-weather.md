# Wi-Fi Hotspot & Weather Spec

## Status: Partially Implemented (Wi-Fi + weather fetch on ESP32 gateway)

> **Design note:** Companion orb weather display and ESP-NOW transport have been removed.
> The companion_orb is deferred. Weather data now flows from the ESP32 gateway to the
> web dashboard via WebSocket JSON, not via ESP-NOW to the orb's GC9A01.

## Objective

Connect `dash_35` (legacy) to an iPhone personal hotspot. Fetch weather data and include it in the WebSocket telemetry broadcast so the web dashboard can display ambient conditions.

## Architecture

```
iPhone Hotspot (Wi-Fi AP)
    |
    v
dash_35 (legacy) (Wi-Fi STA + WebSocket broadcast)
    |  fetches weather via HTTPS every 10 min
    |  includes weather fields in JSON broadcast
    v
Web dashboard (browser on phone)
    |  displays weather in info bar or future weather panel
```

## Wi-Fi Connection

### Credentials

- Phase 1: hardcoded SSID/password in `include/wifi_config.h` (gitignored).
- Phase 2 (future): BLE provisioning for dynamic credentials.

### Connection Behavior

- Attempt Wi-Fi connection during `setup()` with a 10-second timeout.
- If connection fails, continue in offline mode.
- Retry every 60 seconds in the background.
- iPhone hotspot sleeps when idle — handle disconnection gracefully.

### mDNS

- Register `carconsole.local` (or `dash.local`) via mDNS for browser access.

## Weather API

### Provider: Open-Meteo

- Free, no API key required.
- Endpoint: `https://api.open-meteo.com/v1/forecast`
- Parameters: `latitude`, `longitude`, `current_weather=true`
- Response: JSON with `temperature`, `weathercode`

### Location

- Hardcoded lat/lon in `wifi_config.h`.

### Fetch Cadence

- Every 10 minutes while Wi-Fi is connected.
- First fetch 5 seconds after Wi-Fi connects.
- Cache last result.

### WMO Weather Codes (subset)

| Code | Condition |
|------|-----------|
| 0 | Clear sky |
| 1–3 | Partly cloudy |
| 45, 48 | Fog |
| 51–55 | Drizzle |
| 61–65 | Rain |
| 71–75 | Snow |
| 80–82 | Rain showers |
| 95–99 | Thunderstorm |

## WebSocket Payload Extension

Include weather fields in the JSON telemetry broadcast:

```json
{ "spd": 42, "rpm": 2100, "wt": 24, "wc": 0, "wifi": 1, ... }
```

| Field | Type | Description |
|-------|------|-------------|
| `wt` | int | Weather temperature °C, absent when unknown |
| `wc` | int | WMO weather code, absent when unknown |
| `wifi` | bool | 1 when hotspot link is active |

## File Structure

```
include/
  wifi_config.h          # SSID, password, lat/lon (gitignored)
  wifi_config.example.h  # template with placeholder values (committed)
src/dash_35/app/
  wifi_manager.h / .cpp  # Wi-Fi connection + reconnection logic
  weather_client.h / .cpp # HTTP fetch + JSON parse
```

## Dependencies

- `ArduinoJson` — in `[env:dash_35]` lib_deps.
- `HTTPClient` / `WiFi` — built into Arduino ESP32 framework.

## Out Of Scope

- companion_orb weather display (orb deferred)
- ESP-NOW weather transport
- Dynamic location updates (no GPS in scope)
- Weather alerts / multi-day forecast
