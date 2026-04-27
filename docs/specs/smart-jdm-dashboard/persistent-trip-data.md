# Persistent Trip Data Spec

## Status: Planned

## Objective

Save trip metrics and driving statistics to non-volatile storage so they survive power cycles and reboots. Currently all trip data resets when the dash_35 (legacy) loses power.

## Storage Target

- Use ESP32 NVS (Non-Volatile Storage) on the dash_35 (legacy).
- NVS is flash-based key-value storage built into the ESP-IDF/Arduino framework.
- No external SD card or SPIFFS filesystem required for this scope.

## Data To Persist

### Trip Metrics (per-trip, resettable)

- `trip_distance_km` (float)
- `harsh_acceleration_count` (uint8_t)
- `harsh_braking_count` (uint8_t)
- `smoothness_score` (uint8_t)
- `speed_consistency_score` (uint8_t)
- `trip_start_ms` (uint32_t, relative to boot — used for duration calc)

### Lifetime Metrics (cumulative, never reset)

- `total_distance_km` (float)
- `total_trips` (uint16_t)
- `total_drive_time_minutes` (uint32_t)
- `best_smoothness_score` (uint8_t)

### Settings

- `last_fuel_level_pct` (uint8_t) — restore fuel display before first OBD read
- `headlights_on` (bool) — restore last known light/dark mode before OBD read

## Write Strategy

- Write trip metrics to NVS every 60 seconds while driving (speed > 0).
- Write immediately on OBD disconnect (engine off detection).
- Write lifetime metrics only at trip end (OBD disconnect or explicit reset).
- Avoid writing every loop cycle to reduce flash wear.

## Read Strategy

- Read all persisted values during `setup()` before the first render frame.
- Populate `AppState` with restored values so the display shows last-known data immediately.

## Reset Behavior

- Trip metrics reset when OBD transitions to Live (new trip detected).
- Lifetime metrics never reset automatically.
- A manual reset option can be added later via touch screen or BOOT button long-press.

## API Surface

```cpp
namespace app::storage {
  void begin();                          // Init NVS, read all values
  void saveTripMetrics(const TripMetrics &trip);
  void saveLifetimeMetrics(const LifetimeMetrics &lifetime);
  void saveSettings(uint8_t fuel_pct, bool headlights);
  TripMetrics loadTripMetrics();
  LifetimeMetrics loadLifetimeMetrics();
}
```

## Dependencies

- `Preferences.h` (Arduino ESP32 NVS wrapper) — already available in the framework.
- No additional lib_deps required.

## Out Of Scope

- SD card logging
- Cloud sync
- Companion orb persistence (orb is stateless, receives everything from dash)
