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
- Location hardcoded in `wifi_config.h`; no GPS hardware required.

## Good Day to Fill

Track a rolling price history to give the driver a simple fill/skip signal.

### Price History Storage

- Store up to 14 daily price samples in NVS under namespace `fuel_hist`.
- Each sample is a `uint16_t` (price in tenths of a currency unit, e.g. 1580 = ¥158.0/L).
- One sample recorded per successful fetch (at most once per 30 minutes, so one per refuel opportunity).

### Judgment Logic

| Condition | Display | Color |
|-----------|---------|-------|
| Price ≤ 7-day avg − threshold | `GOOD DAY ↓` | Green |
| Price within ±threshold of avg | `NORMAL` | White |
| Price ≥ 7-day avg + threshold | `HIGH TODAY ↑` | Amber |
| Fewer than 3 samples collected | `CHECKING...` | Gray |

- Default threshold: `3` (configurable as `FUEL_PRICE_GOOD_THRESHOLD` in `wifi_config.h`).
- Displayed beneath the cheapest station row in detail view.

```
| Cheapest Fuel | ¥158/L  ENEOS 1.2km |
| Price Trend   | GOOD DAY ↓          |
```

### NVS Usage

- Write one new sample after each successful fetch if price has changed.
- Cap history at 14 entries (circular buffer, oldest overwritten).
- Shared namespace with `app::storage` Preferences instance.

## Out Of Scope

- Navigation to the station
- Price comparison UI with multiple stations
