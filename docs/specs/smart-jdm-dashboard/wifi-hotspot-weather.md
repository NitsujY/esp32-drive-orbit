# Wi-Fi Hotspot & Weather Display Spec

## Status: Planned

## Objective

Connect the dash_35 to an iPhone personal hotspot for internet access. Fetch live weather data and display it on the companion_orb as an ambient weather indicator integrated with the companion face.

## Architecture

```
iPhone Hotspot (Wi-Fi AP)
    |
    v
dash_35 (Wi-Fi STA + ESP-NOW TX)
    |  fetches weather via HTTPS
    |  adds weather to telemetry
    |  broadcasts via ESP-NOW
    v
companion_orb (ESP-NOW RX)
    |  displays weather on GC9A01
```

- Wi-Fi STA and ESP-NOW coexist on the same ESP32 radio.
- OBD Bluetooth Classic also coexists (separate radio).
- All three links (OBD BT, ESP-NOW, Wi-Fi) run simultaneously.

## Wi-Fi Connection

### Credentials

- Phase 1: hardcoded SSID/password in a config header (e.g. `include/wifi_config.h`).
- Phase 2 (future): captive portal or BLE provisioning for dynamic credentials.
- The config header should be `.gitignore`d to avoid committing personal credentials.

### Connection Behavior

- Attempt Wi-Fi connection during `setup()` with a 10-second timeout.
- If connection fails, continue without internet (offline mode).
- Retry connection every 60 seconds in the background.
- iPhone hotspot sleeps when idle — handle disconnection gracefully.
- Log Wi-Fi status to Serial: connected, disconnected, reconnecting.

### mDNS

- Register `dash.local` via mDNS for easy browser access (future web dashboard).

## Weather API

### Provider: Open-Meteo

- Free, no API key required.
- Endpoint: `https://api.open-meteo.com/v1/forecast`
- Parameters: `latitude`, `longitude`, `current_weather=true`
- Response: JSON with `temperature`, `weathercode`, `windspeed`, `winddirection`

### Location

- Phase 1: hardcoded lat/lon in `wifi_config.h` (your home/city coordinates). This is sufficient for the weather display; no GPS hardware is required.
- Phase 2 (optional): companion phone or Wi‑Fi geolocation could provide live coordinates for improved accuracy, but it is not required.

### Fetch Cadence

- Fetch weather every 10 minutes (weather doesn't change fast).
- First fetch 5 seconds after Wi-Fi connects.
- Cache last successful response — display cached data if fetch fails.
- Use `HTTPClient` from the Arduino ESP32 framework (no extra lib_deps).

### Response Parsing

- Parse JSON using `ArduinoJson` library (add to `lib_deps`).
- Extract: `temperature` (float °C), `weathercode` (int, WMO code).

### WMO Weather Codes (subset for display)

| Code | Condition | Orb Icon |
|------|-----------|----------|
| 0 | Clear sky | ☀ sun |
| 1-3 | Partly cloudy | ⛅ cloud-sun |
| 45, 48 | Fog | 🌫 fog |
| 51-55 | Drizzle | 🌧 light rain |
| 61-65 | Rain | 🌧 rain |
| 71-75 | Snow | ❄ snow |
| 80-82 | Rain showers | 🌧 heavy rain |
| 95-99 | Thunderstorm | ⛈ storm |

## Telemetry Extension

### New Fields in DashboardTelemetry

```cpp
// Weather data (from internet, updated every 10 min)
int8_t weather_temp_c;        // outdoor temperature, -128 = unknown
uint8_t weather_code;         // WMO weather code, 255 = unknown
bool wifi_connected;          // true when hotspot link is active
```

### Transport

- Add weather fields to `StatusTelemetryPayload` (slow-refresh, fits naturally).
- Update `StatusTelemetryPayload` struct, static_assert, encoder, and parser.

## Orb Weather Display

### Integration with Companion Face

When weather data is available (weather_code != 255):

**Chill mode**: show a small weather icon + temperature below the coaching score.

```
    [face]
     82
   smooth
   ☀ 24°C
```

**Sport mode**: weather is hidden (gear + RPM take priority).

**Sleep mode**: show weather prominently since there's nothing else to display.

```
   z z
  [closed eyes]
   ☀ 24°C
```

### Weather Icons (pixel art on 240x240)

- Draw simple geometric shapes — no bitmaps needed.
- Sun: filled circle with short radiating lines.
- Cloud: two overlapping filled circles.
- Rain: cloud + vertical line drops below.
- Snow: cloud + small dots below.
- Storm: cloud + zigzag lightning line.

### Face Mood Integration

- Rain/storm: face looks slightly worried (eyebrows angle down).
- Clear/sunny: face is slightly happier (wider smile).
- This is a subtle overlay on top of the existing speed-based mood system.

## Offline Behavior

- If Wi-Fi never connects: weather fields stay at unknown (255/-128).
- Orb skips weather display entirely when data is unknown.
- No error messages on screen — just absence of weather info.
- All other features (OBD, ESP-NOW, face, coaching) work normally.

## File Structure

```
include/
  wifi_config.h          # SSID, password, lat/lon (gitignored)
  wifi_config.example.h  # template with placeholder values (committed)
src/dash_35/app/
  wifi_manager.h         # Wi-Fi connection + reconnection logic
  wifi_manager.cpp
  weather_client.h       # HTTP fetch + JSON parse
  weather_client.cpp
```

## Dependencies

- `ArduinoJson` — add to `[env:dash_35]` lib_deps.
- `HTTPClient` — built into Arduino ESP32 framework.
- `WiFi` — built into Arduino ESP32 framework.

## Out Of Scope

- Device-provided location (future spec) — e.g., companion phone or Wi‑Fi geolocation
- Weather alerts/notifications
- Multi-day forecast
- Phone companion app
- Web dashboard served from ESP32 (separate future spec)
