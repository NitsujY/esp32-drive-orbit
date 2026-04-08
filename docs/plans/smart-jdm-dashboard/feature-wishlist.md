# Feature Wishlist

## Status: Ideas — not yet specced or scheduled

> **Design context:** dash_35 is now a headless OBD telemetry gateway. The web dashboard (browser/iOS)
> is the primary UI. companion_orb is deferred. Features below target the **web dashboard** unless
> noted as firmware-side.

All GPS-dependent features are excluded from this project (no GPS hardware in scope). Location-aware features use a hardcoded pre-config coordinate in `wifi_config.h`.

---

## No GPS, No Internet Required

These features run entirely from OBD data and NVS. They work with the iPhone hotspot off.

### 1. Driver Behavior Scoring (web dashboard)

~~Driver Behavior Scoring + Orb Emotions~~ — Orb emotions removed (companion_orb deferred).

Scoring is **now implemented** in the web dashboard trip-summary-overlay.
See `specs/driver-behavior-scoring.md`.

### 2. Fuel Efficiency Tracker (km/L per trip)

Derive fuel burn from the delta between `fuel_level_pct` at trip start and end, divided by `trip_distance_km`. Display instant km/L in the trip summary sheet. Store `best_efficiency` in NVS. No internet needed.

### 3. Maintenance Reminder by Odometer

Accumulate total km from all trips in NVS (`total_distance_km`). Prompt the driver at configurable intervals (e.g. every 5000 km for oil, 10000 km for tires). Display a notification in the web dashboard. Thresholds configured in vehicle profile. No internet needed.

### 4. Cold-Start Protection Reminder

When coolant temp < 60°C, display a "LET IT WARM" banner on the web dashboard. ~~Orb WORRIED mode removed.~~ Prevents the driver from flooring it on a cold engine.

### 5. Battery Voltage Trend Alert + SoC Algorithm

Track `battery_voltage` over the last 10 minutes and compute a proper **State of Charge (SoC)** percentage from a voltage lookup table. No internet needed.

#### SoC Lookup Table (engine OFF, resting voltage)

12V lead-acid batteries have a well-characterised open-circuit voltage curve. Linear interpolation between these ten nodes gives ±3% accuracy:

| Voltage (V) | SoC (%) |
| ----------- | ------- |
| ≥ 12.73 | 100 |
| 12.62 | 90 |
| 12.50 | 80 |
| 12.37 | 70 |
| 12.24 | 60 |
| 12.10 | 50 |
| 11.96 | 40 |
| 11.81 | 30 |
| 11.66 | 20 |
| 11.51 | 10 |
| ≤ 11.36 | 0 |

```cpp
// Linear interpolation over the table above
uint8_t voltageToSoC(float volts) {
    static constexpr float V[] = {12.73f,12.62f,12.50f,12.37f,12.24f,
                                   12.10f,11.96f,11.81f,11.66f,11.51f,11.36f};
    static constexpr uint8_t S[] = {100,90,80,70,60,50,40,30,20,10,0};
    if (volts >= V[0]) return 100;
    if (volts <= V[10]) return 0;
    for (int i = 0; i < 10; i++) {
        if (volts >= V[i + 1]) {
            float t = (volts - V[i + 1]) / (V[i] - V[i + 1]);
            return (uint8_t)(S[i + 1] + t * (S[i] - S[i + 1]));
        }
    }
    return 0;
}
```

#### Measurement Timing

- **While engine is running**: alternator dominates voltage (13.4–14.7V) — SoC cannot be read from raw voltage. Display `CHARGING` with alternator voltage instead.
- **Resting sample (preferred)**: the 1–2 second window after OBD first connects before RPM stabilises gives a clean pre-load reading. Capture and cache this.
- **Post-stop sample**: when RPM drops to 0 and speed reaches 0, schedule a voltage read after a 60-second settle delay for the most accurate SoC.

#### Alert Conditions

| Condition | Trigger | Display |
| --------- | ------- | ------- |
| Weak battery | SoC < 50% at engine start | `BATTERY WEAK` banner |
| Charging fault | Alternator voltage < 13.4V at speed > 20 km/h | `CHARGING FAULT` |
| Over-voltage | Alternator voltage > 15.0V | `OVERVOLT` |
| Deep discharge | SoC < 20% | `BATTERY CRITICAL` |

### 6. Daily Driving Challenge (Local)

Generate a local daily challenge based on the current date (seeded RNG from epoch day). Examples:

- "No harsh braking today"
- "Keep RPM under 2500 for the whole trip"
- "Zero over-speed events"

Badge shown in the web dashboard trip summary if the challenge is met at trip end. No internet needed.

### 7. Engine Idle Time Budget

Track total idle time per trip (speed == 0, engine on). Show a small idle counter in the trip summary sheet. If idle exceeds 5 minutes in one session, show `IDLE 6:12` in the web dashboard. ~~Orb BORED reaction removed.~~

---

## Needs Internet (iPhone Hotspot)

These features use the existing Wi-Fi + iPhone hotspot path. Location is pre-configured in `wifi_config.h`.

### 8. Air Quality Index (AQI) Display

**API:** Open-Meteo Air Quality (`https://air-quality-api.open-meteo.com/v1/air-quality`)

- Parameters: `latitude`, `longitude`, `current=pm10,pm2_5,european_aqi`
- Free, no API key.
- Fetch every 30 minutes.
- Display AQI in the web dashboard info area: `AQI 42 GOOD`.
- Pre-configured lat/lon, no GPS needed.

### 9. UV Index Display

**API:** Open-Meteo Forecast (same fetch as weather, add `hourly=uv_index`).

- Zero extra fetch cost — piggyback on existing weather call.
- Display UV level in the web dashboard: `UV 7 HIGH`.

### 10. ~~Fuel Price "Good Day to Fill"~~ (spec removed)

~~`fuel-price-lookup.md` has been removed~~ — spec was written for the physical dash_35 detail view which no longer exists. Can be re-specced for the web dashboard if needed.

### 11. ~~Speed Camera Database~~  (spec removed)

~~`speed-camera-alerts.md` has been removed~~ — spec targeted orb alert display and physical TFT top bar. Can be re-specced as a web dashboard notification if needed.

### 12. Vehicle Recall Check

**API:** NHTSA vPIC API (`https://api.nhtsa.dot.gov/recalls/recallsByVehicle`)

- Parameters: `make`, `model`, `modelYear` (sourced from vehicle profile).
- Free, no API key.
- Fetch once per day.
- If open recalls exist, show `RECALL!` badge in the web dashboard with count.
- Cache result in NVS for 24 hours.

### 13. Weather-Based Driving Advisory

Extend the existing weather display with a context-aware advisory:

| Condition | Advisory |
| --------- | -------- |
| Rain / storm | `WET ROADS — take care` |
| Temperature < 3°C | `ICE RISK — slow down` |
| Heavy wind (> 50 km/h) | `STRONG WIND` |
| Clear + hot (> 35°C) | `TYRE PRESSURE CHECK` |

Message shown as a one-line banner in the web dashboard. ~~Orb chill mode message removed.~~

### 14. Local Sunrise / Sunset for Auto Dark Mode

**API:** Open-Meteo Forecast (add `daily=sunrise,sunset`).

- Zero extra fetch cost — extend existing weather call.
- Auto-switch to dark mode on the web dashboard when current time is after sunset or before sunrise.
- Trigger a headlight reminder when the vehicle is moving and `headlights_on` is still false.

---

## Notes

- All "hardware GPS" ideas (turn-by-turn navigation, live speed camera distance, live traffic) are out of scope — this project has no GPS module.
- Hotspot-dependent features degrade gracefully: they simply hide their UI element when Wi-Fi is disconnected.
- Features 8–14 share the same Wi-Fi event loop and can be batched into one HTTP task without adding new connection logic.
- companion_orb emotion features have been removed from all entries above.