# NTP Time Sync Spec

## Status: Planned

## Objective

Synchronize accurate time from the internet via NTP so both displays can show a clock. The orb shows time during sleep mode as its primary idle display. The dash shows time in the top bar.

## NTP Configuration

- NTP server: `pool.ntp.org` (default, works globally).
- Timezone: configurable in `wifi_config.h` (e.g., `JST-9` for Japan, `CST-8` for Taiwan).
- Use `configTime()` from the Arduino ESP32 framework — built-in, no extra libraries.
- Sync once on Wi-Fi connect, then every 6 hours.

## Time Display

### Companion Orb — Sleep Mode Clock

When the orb is in sleep mode (parked > 20s), replace the Zzz face with a clock:

```
     10:42
   [closed eyes]
     24°C
```

- Large centered time in HH:MM format.
- Closed eyes below (still breathing).
- Weather temperature below if available.
- Mood ring dims to a slow, gentle pulse.
- Updates every minute (not every second — saves redraws).

### Companion Orb — Active Mode

- No clock shown during active driving (screen space is for face/gear/score).

### Dash_35 — Top Bar

- Show time in the top-right corner, replacing or alongside the OBD status text.
- Small text, muted color — glanceable but not distracting.
- Format: `HH:MM` (24-hour).

## Telemetry Extension

- Time is not transmitted via ESP-NOW.
- Both boards sync independently when Wi-Fi is available.
- The orb gets time from the dash_35 indirectly: add `uint32_t epoch_seconds` to `StatusTelemetryPayload`.
- The orb uses the received epoch to display time without needing its own Wi-Fi/NTP.

```cpp
uint32_t epoch_seconds;  // Unix timestamp from dash_35, 0 = unknown
```

## Offline Behavior

- If Wi-Fi never connects, `epoch_seconds` stays 0 and no clock is shown.
- If Wi-Fi connects once then disconnects, the ESP32's internal RTC keeps time until reboot.
- On reboot without Wi-Fi, time is unknown again (no hardware RTC).

## Dependencies

- Wi-Fi hotspot connection.
- No additional libraries (configTime is built-in).

## Out Of Scope

- Hardware RTC module (DS3231 etc.)
- Alarm/timer features
- Date display
- Stopwatch/lap timer (could be a separate spec)
