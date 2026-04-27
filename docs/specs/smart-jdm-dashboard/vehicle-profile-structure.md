# Vehicle Profile Structure

## Purpose

This document defines how car-specific OBD2 behavior is organized for `dash_35` (legacy) so BLE target names and derived calculations do not live directly inside the transport code.

## Active Profile

- Profile id: `toyota_sienta`
- Display name: `Toyota Sienta`

## Folder Structure

```text
src/
  dash_35/
    app/
      elm327_client.h
      elm327_client.cpp
      vehicle_profiles/
        vehicle_profile.h
        vehicle_profile.cpp
```

## BLE Connect Targets

The active Toyota Sienta profile currently attempts Bluetooth Classic connection using these adapter names in order:

1. `OBDII`
2. `IOS-Vlink`
3. `VLINK`
4. `ELM327`

## Active OBD2 Formulas Used

These are the active calculations associated with the Sienta profile in the repo:

1. Engine RPM: `010C` -> `((A * 256) + B) / 4`
2. Vehicle Speed: `010D` -> `A`, then apply profile `speed_offset_kph`, then clamp to `>= 0`
3. Coolant Temperature: `0105` -> `A - 40`
4. Fuel Level: `ATSH7C0` + `2129` -> liters = `A / 2`, then percent = `(liters / 42.0) * 100`
5. Battery Voltage: `ATRV` -> `volts * 1000`

The current `dash_35` (legacy) profile does not poll standard fuel PID `012F` because it is known not to work on this Toyota Sienta.

Fuel calibration is profile-based:

1. `speed_offset_kph` adds a constant offset after parsing `010D`
1. `toyota_fuel_header` sets the ECU header used before `2129`
1. `toyota_fuel_offset_liters` adds a fixed liters offset after `A / 2`
1. `fuel_scale_percent` applies a final percent scale before clamping to `0..100`

Trip distance is accumulated in the app-state layer from live vehicle speed using time integration:

$$
\Delta d_{km} = speed_{kph} \cdot \Delta t_h
$$

The live trip resets when OBD transitions into a live connected state, which keeps on-car trip distance separate from any simulator activity.

## Derived Sienta Range Estimate

The active Sienta profile currently uses:

1. Fuel tank capacity: `42.0 L`
2. Baseline efficiency: `16.5 km/L`
3. Speed-adjusted factor for range estimate

The formula is implemented in the vehicle profile layer rather than the ELM327 transport layer so it can be tuned per car later.

## Battery Level Guessing Algorithm

Yes. A practical dashboard-side battery estimation algorithm is possible, but it should be treated as a **heuristic State of Charge (SoC)** estimate rather than a true battery-management reading.

### Recommended Approach

1. Use **resting voltage** as the primary SoC signal.
2. Use **charging voltage** only to detect alternator health, not battery percentage.
3. Smooth readings over time and publish both an SoC estimate and a confidence level.

### Step 1: Capture the Best Voltage Sample

Preferred sample windows:

1. Engine off, 30 to 60 seconds after shutdown.
2. Immediately before the engine settles into charging voltage.
3. Long idle with alternator load known to be low, but with lower confidence.

### Step 2: Convert Resting Voltage to SoC

Use a lookup table with linear interpolation:

| Resting Voltage | Estimated SoC |
| --------------- | ------------- |
| >= 12.73V | 100% |
| 12.62V | 90% |
| 12.50V | 80% |
| 12.37V | 70% |
| 12.24V | 60% |
| 12.10V | 50% |
| 11.96V | 40% |
| 11.81V | 30% |
| 11.66V | 20% |
| 11.51V | 10% |
| <= 11.36V | 0% |

### Step 3: Charging-State Logic

When engine RPM is above idle and measured voltage is between about 13.4V and 14.8V:

1. Freeze the last known SoC estimate.
2. Report charging health instead:
  `LOW CHARGE`, `NORMAL CHARGE`, or `OVERVOLT`.

Suggested thresholds:

1. `< 13.4V`: charging weak
2. `13.4V - 14.8V`: charging normal
3. `> 15.0V`: over-voltage warning

### Step 4: Confidence Model

Use a simple confidence score:

1. `HIGH`: engine off resting sample
2. `MEDIUM`: just-before-charge sample
3. `LOW`: estimate held during active charging

### Step 5: UI Recommendation

In the main driving cluster:

1. Do not show percentage continuously.
2. Only surface battery information in rolling status or health/detail view.

Suggested outputs:

1. `BATTERY 78%`
2. `BATTERY LOW`
3. `CHARGING 13.8V`
4. `CHARGING FAULT`
