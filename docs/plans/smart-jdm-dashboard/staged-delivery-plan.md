# Smart JDM Dashboard Staged Delivery Plan

## Purpose

This document separates pre-implementation validation from firmware coding so hardware and protocol risks are discovered early.

## Current Status

- Stage 3 is approved, including the default preview theme set.
- Stage 4 is approved and complete as an interaction-validation checkpoint.
- Stage 5 is complete. Toyota Sienta PID reads validated on hardware; live OBD telemetry is the primary source.
- Stage 6 is active. Inter-board transport uses ESP-NOW (wireless, no UART wiring required).
- The `dash_35` transmitter broadcasts framed telemetry packets via ESP-NOW.
- The `companion_orb` receives ESP-NOW packets and feeds them into the existing stream parser.
- Remaining Stage 6 work: GC9A01 rendering on the Slave, live telemetry exchange validation.

### Light/Dark Mode

- Both boards support light mode (default, headlights OFF) and dark mode (headlights ON).
- The `headlights_on` field in `DashboardTelemetry` controls the mode.
- The dash_35 sets `theme::darkMode()` from the telemetry each loop cycle.
- The companion_orb receives `headlights_on` via the fast telemetry ESP-NOW packet and sets its own `theme::darkMode()`.
- Light mode: light gray backgrounds, darker accents for daytime readability.
- Dark mode: near-black backgrounds, bright neon accents for night driving.
- The Toyota headlight PID is pending discovery via the `toyota-scan` probe tool. Once found, the ELM327 client will set `headlights_on` from the OBD response.

## Stages

### Stage 0: Documentation

Outputs:

- Foundation specification
- Delivery plan
- Task list
- Hardware verification checklist

Why it exists:

- Freeze design decisions before code changes continue
- Keep hardware constraints visible
- Give the repo a durable workflow for spec-driven development

### Stage 1: Toolchain Installation

Outputs:

- PlatformIO core available locally
- Espressif32 platform packages installed locally
- Arduino framework packages installed locally

Why it exists:

- Avoid mixing environment setup failures with firmware defects
- Make later compile failures attributable to code, not missing dependencies

### Stage 2: Hardware Verification

Outputs:

- Confirmed `dash_35` boot behavior
- Confirmed `dash_35` serial-console visibility
- Confirmed `dash_35` display and touch baseline
- Confirmed `dash_35` PSRAM availability

Why it exists:

- Prevent firmware debugging on unstable primary hardware
- Catch power, cable, display, touch, or PSRAM issues early

### Stage 3: Pure UI Preview

Outputs:

- Local dashboard UI preview in VS Code
- Local companion display UI preview in VS Code
- Theme and motion validation before embedded UI implementation
- Simulated telemetry driving the preview experience

Why it exists:

- UI changes are cheaper before device-specific implementation
- You can validate the futuristic minimalist direction before firmware complexity increases
- The dashboard and orb can be designed as a coherent pair early

### Stage 4: Interaction Validation

Outputs:

- Approved companion-board role split
- Approved no-Wi-Fi first-stage scope
- Approved deferred transport decision
- Approved `companion_orb` secondary display role

Why it exists:

- Prevent UI and transport work from being coupled too early
- Allow parser and transport behavior to be tested in isolation

### Stage 5: Firmware Implementation

Outputs:

- Working `platformio.ini`
- `dash_35` standalone firmware path
- Welcome and boot flow active on device
- OBD connection-state UX from disconnected through live data
- Embedded dashboard layout that clears the bottom toolbar cleanly
- Shared transport header ready for later reuse
- Deferred companion transport and slave integration path

Why it exists:

- Implementation starts only after the environment and hardware are known-good

### Stage 6: Companion Linkage And Board Integration

Outputs:

- Physical or selected inter-board transport hookup
- `companion_orb` live packet reception from `dash_35`
- Companion rendering and behavior validated on hardware

Why it exists:

- Keeps `dash_35` delivery moving without blocking on secondary-board integration
- Preserves the transport work already completed while moving it to the end of the delivery path

## Gate Policy

Stage 5 started after explicit user approval on 2026-03-25.
