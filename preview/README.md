# Stage 3 Preview Bootstrap

This folder contains the first local UI preview slice for Stage 3.

## What It Includes

- `dashboard_preview/`: a host-side dashboard prototype sized for 320x480 behavior
- `companion_preview/`: a host-side orb companion prototype sized for 240x240 behavior
- `shared/telemetry.js`: a shared simulated telemetry stream used by both previews
- `index.html`: a landing page that links to both previews

## Why This Exists

This is a fast host-side bootstrap for validating flow, hierarchy, sport-mode behavior, and theme direction before embedded UI integration.

It does not lock the project into the final embedded rendering stack.

## Run Locally

From the repository root:

```sh
python3 -m http.server 8123 --directory preview
```

Then open:

- `http://127.0.0.1:8123/`
- `http://127.0.0.1:8123/dashboard_preview/`
- `http://127.0.0.1:8123/companion_preview/`

## Current Stage 3 Scope

- local preview targets exist
- both previews share a simulated telemetry source
- sport-mode transition is visible
- trip and health information architecture is represented
- the orb preview uses a circular mask and reaction states

## Remaining Stage 3 Work

- refine the welcome sequence timing and motion
- review the default theme set
- decide whether to keep this bootstrap as a design harness or replace it with an LVGL + SDL2 simulator
