# Native Dashboard Preview

This target is the primary dashboard review path on macOS.

It renders the smart JDM dashboard in a native AppKit window and reuses the shared dashboard presentation code that now also feeds the embedded renderer.

## Build

From the repository root:

```sh
cmake -S tools/dashboard_preview_native -B build/dashboard_preview_native
cmake --build build/dashboard_preview_native
```

## Run

```sh
./build/dashboard_preview_native/dashboard_preview_native
```

This executable is the macOS native preview. It is separate from the browser previews under `preview/`.

## Controls

- `D` toggles the trip-detail screen
- `N` toggles the night palette
- `L` toggles the reminder-zone demo
- `Space` pauses or resumes the simulation
- `R` resets the simulation

## Current Preview

The native preview currently renders two local review states:

- the screenshot-inspired sport drive cluster with a compact weather block at top-left, a rolling status line in the top center, a dedicated reminder capsule below it, a central speed frame with right-aligned unit text, a tach ribbon, a lower-left drivetrain status block, and a merged bottom-right energy field
- a trip-detail screen with fuel-saving, comfort, flow, and overall trip scoring plus a mock 0-60 MAF reference strip

## Scope

- Native dashboard-only preview for macOS
- Shared C++ snapshot/presentation layer with the embedded dashboard renderer
- Local simulated telemetry loop for rapid look-and-feel review

## Current Boundary

The shared layer covers dashboard state, labels, chips, colors, and formatted view data.

The embedded renderer is still the current Arduino_GFX smoke-test implementation, not LVGL yet. Moving both the device and the native preview onto a single LVGL widget tree is the next step if full widget-level parity is required.
