# Fuel Price Lookup Spec

## Status: Planned

## Objective

Fetch nearby gas station prices and display the cheapest option with distance on the dash_35 detail view. Helps the driver decide where to refuel, especially when the low-fuel alert is active.

## Data Source

### Option A: Google Places API (Nearby Search)

- Search for `gas_station` type near current coordinates.
- Returns name, location, and sometimes price (varies by region).
- Requires API key with billing (free tier: $200/month credit).

### Option B: Japan-specific fuel price API

- For Japan/Taiwan markets, government or crowd-sourced fuel price APIs may be more accurate.
- e.g., `gogo.gs` (Japan), `gasbuddy.com` (US/Canada).
- Phase 1: use a configurable API endpoint in `wifi_config.h`.

### Recommended Phase 1

- Use a simple configurable REST endpoint that returns JSON with station name, price, lat/lon.
- This allows swapping the backend without firmware changes.
- Default to a mock/static endpoint for development.

## Display Location

- Shown on the dash_35 detail view (BOOT button / touch toggle).
- New row below the existing health metrics:

```
| Cheapest Fuel | ¥158/L  ENEOS 1.2km |
```

- Only shown when:
  - Wi-Fi is connected AND fuel data is available.
  - Fuel level is below 30% (or always in detail view).

## Fetch Strategy

- Fetch every 30 minutes (fuel prices don't change frequently).
- Fetch immediately when fuel drops below 20%.
- Cache last result.

## Telemetry Extension

- Fuel price data stays on the dash_35 only (not broadcast to orb).
- No changes to ESP-NOW telemetry payload.

## Dependencies

- Wi-Fi hotspot connection.
- ArduinoJson.
- GPS module (future) for accurate current location. Phase 1 uses hardcoded coordinates.

## Out Of Scope

- Navigation to the station
- Price comparison UI with multiple stations
- Fuel price history/trends
