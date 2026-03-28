# Vehicle Profile Structure

## Purpose

This document defines how car-specific OBD2 behavior is organized for `dash_35` so BLE target names and derived calculations do not live directly inside the transport code.

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
2. `V-LINK`
3. `VLINK`
4. `ELM327`

## Active OBD2 Formulas Used

These are the active calculations associated with the Sienta profile in the repo:

1. Engine RPM: `010C` -> `((A * 256) + B) / 4`
2. Vehicle Speed: `010D` -> `A`, then apply profile `speed_offset_kph`, then clamp to `>= 0`
3. Coolant Temperature: `0105` -> `A - 40`
4. Fuel Level: `ATSH7C0` + `2129` -> liters = `A / 2`, then percent = `(liters / 42.0) * 100`
5. Battery Voltage: `ATRV` -> `volts * 1000`

The current `dash_35` profile does not poll standard fuel PID `012F` because it is known not to work on this Toyota Sienta.

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
