# Stage 3 Preview Bootstrap

This folder contains the legacy browser bootstrap for the first local UI preview slice.

## What It Includes

- `dashboard_preview/`: the original browser mock for the dashboard
- `companion_preview/`: a host-side orb companion prototype sized for 240x240 behavior
- `shared/telemetry.js`: a shared simulated telemetry stream used by both previews
- `index.html`: a landing page that links to both previews
- `../tools/dashboard_preview_native/`: the native macOS dashboard preview that now replaces the browser dashboard as the primary review path

## Why This Exists

The browser preview was useful for early direction work, but it is no longer the recommended dashboard review path.

The preferred dashboard preview is now the native macOS simulator under `tools/dashboard_preview_native`, backed by shared C++ dashboard presentation code that is also used by the embedded renderer.

## Run Locally

For the dashboard preview on macOS, from the repository root:

```sh
cmake -S tools/dashboard_preview_native -B build/dashboard_preview_native
cmake --build build/dashboard_preview_native
./build/dashboard_preview_native/dashboard_preview_native
```

Keyboard controls:

- `D`: toggle detail view
- `N`: toggle night palette
- `Space`: pause or resume simulation
- `R`: reset simulation

For the legacy browser bootstrap:

```sh
python3 -m http.server 8123 --directory preview
```

Then open:

- `http://127.0.0.1:8123/`
- `http://127.0.0.1:8123/dashboard_preview/`
- `http://127.0.0.1:8123/companion_preview/`

## Current Stage 3 Scope

- native macOS dashboard preview exists
- dashboard preview now uses a native C++ telemetry harness instead of the browser telemetry mock
- sport-mode transition is visible
- trip and health information architecture is represented
- the orb preview uses a circular mask and reaction states

## Remaining Stage 3 Work

- refine the welcome sequence timing and motion
- review the default theme set
- migrate the native preview and embedded renderer onto a common LVGL widget tree when the project moves past the current Arduino_GFX smoke-test phase
