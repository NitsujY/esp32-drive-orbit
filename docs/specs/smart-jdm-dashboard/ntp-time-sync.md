# NTP Time Sync Spec

## Status: Planned (ESP32 gateway firmware)

> **Design note:** Companion orb clock display has been removed from this spec.
> The companion_orb is deferred. The web dashboard uses browser time natively.
> This spec covers NTP sync on the `dash_35` ESP32 gateway only.

## Objective

Synchronize accurate time via NTP on `dash_35` so it can stamp WebSocket telemetry and the web dashboard can display a reliable clock.

## NTP Configuration

- NTP server: `pool.ntp.org` (default, works globally).
- Timezone: configurable in `wifi_config.h` (e.g., `JST-9` for Japan, `CST-8` for Taiwan).
- Use `configTime()` from the Arduino ESP32 framework — built-in, no extra libraries.
- Sync once on Wi-Fi connect, then every 6 hours.

## Time in WebSocket Payload

Include `epoch_seconds` in the telemetry JSON broadcast so the web dashboard can display time without browser-clock drift:

```json
{ "epoch": 1744000000, "spd": 42, "rpm": 2100, ... }
```

The web dashboard falls back to `Date.now()` when `epoch` is absent or 0.

## Offline Behavior

- If Wi-Fi never connects, time is omitted from the payload (`epoch: 0`).
- If Wi-Fi connects once then disconnects, the ESP32's internal RTC keeps time until reboot.
- On reboot without Wi-Fi, time is unknown; the web dashboard uses browser time.

## Dependencies

- Wi-Fi hotspot connection.
- No additional libraries (`configTime` is built-in).

## Out Of Scope

- Hardware RTC module (DS3231 etc.)
- companion_orb clock display (orb deferred)
- Alarm/timer features
- Date display
