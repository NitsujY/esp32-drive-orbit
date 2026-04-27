# Stage 5 Display Smoke-Test Stack Decision

## Status

Accepted for the current `dash_35` (legacy) Stage 5 slice.

## Decision

Use `GFX_Library_for_Arduino` with the Waveshare `ESP32-Touch-LCD-3.5` board's known ST7796 SPI bring-up path for the first on-device display milestone.

## Why

- The Waveshare board documentation already provides a known-good Arduino example using `Arduino_ST7796`.
- This gives a low-risk hardware validation step before introducing a heavier retained-mode UI framework.
- The project can now verify panel reset, SPI wiring, orientation, backlight, and basic rendering while keeping the existing `dash_35` (legacy) state pipeline unchanged.

## Board Notes

- Display controller: `ST7796`
- SPI pins: `DC=27`, `CS=5`, `SCK=18`, `MOSI=23`, `MISO=19`
- Backlight pin: `25`
- IO expander: `TCA9554` at `0x20`
- Display reset pulse is driven through `TCA9554` pin `0`

## Consequences

- The current renderer is a smoke-test renderer, not the final visual system.
- It is intentionally direct-draw and simple so hardware bring-up problems are isolated from UI-framework problems.
- A later decision is still required for the full embedded UI layer, likely after display stability and touch bring-up are confirmed.
