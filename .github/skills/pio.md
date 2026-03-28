# PlatformIO Build Skill

This project uses [PlatformIO](https://platformio.org/) (CLI: `pio`) to build, flash, and monitor ESP32 firmware.

## Environments

| Environment      | Board      | Source filter                        | Notes                              |
|------------------|------------|--------------------------------------|------------------------------------|
| `dash_35`        | esp32dev   | `src/dash_35/**`                     | Default. 16 MB flash, PSRAM.      |
| `companion_orb`  | esp32dev   | `src/companion_orb/**`               | Placeholder orb firmware.          |

## Common commands

```bash
# Build the default environment (dash_35)
pio run

# Build a specific environment
pio run -e dash_35
pio run -e companion_orb

# Flash to a connected board (auto-detects port)
pio run -e dash_35 --target upload

# Flash to a specific serial port
pio run -e dash_35 --target upload --upload-port /dev/cu.usbserial-0001

# Open serial monitor (115200 baud, as configured)
pio device monitor

# Clean build artifacts
pio run -e dash_35 --target clean

# Full clean (removes .pio/build entirely)
pio run -e dash_35 --target fullclean

# List connected devices
pio device list

# Update / install library dependencies
pio pkg install
```

## Build flags

All environments inherit from `[env]`:
- `-Wall -Wextra` — strict warnings
- `-DCORE_DEBUG_LEVEL=3` — ESP32 log level INFO

The `dash_35` environment adds:
- `-DBOARD_HAS_PSRAM`
- `-mfix-esp32-psram-cache-issue`

## Library dependencies (dash_35)

- Arduino_GFX v1.6.0 (display driver)
- TCA9554 (I²C GPIO expander)
- U8g2 ^2.36.9 (monochrome graphics)

Dependencies are auto-installed into `.pio/libdeps/` on first build.

## Tips

- If the build fails with missing headers, run `pio pkg install` first.
- The `.pio/` directory is gitignored; a fresh clone needs a build to pull deps.
- Monitor and flash can run together: `pio run -e dash_35 --target upload && pio device monitor`.
- Use `pio run -e dash_35 -v` for verbose build output when debugging compile errors.
- The upload speed is set to 921600 baud for fast flashing.
