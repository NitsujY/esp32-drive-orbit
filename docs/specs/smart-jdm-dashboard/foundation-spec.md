# Smart JDM Dashboard Foundation Spec

## Document Control

- Status: Draft
- Date: 2026-03-24
- Project: ESP32 Drive Orbit
- Scope: Foundational firmware architecture for dual-ESP32 dashboard system

## Recommended Spec-Driven Folder Structure

This repository should use a documentation layout that keeps product intent, implementation planning, and delivery tasks separate.

```text
docs/
  specs/
    smart-jdm-dashboard/
      foundation-spec.md
  plans/
    smart-jdm-dashboard/
  tasks/
    smart-jdm-dashboard/
  decisions/
```

Purpose of each folder:

- `docs/specs/`: source of truth for product and system design requirements.
- `docs/plans/`: implementation sequencing, milestones, and build-out strategy.
- `docs/tasks/`: execution-level task breakdown for coding phases.
- `docs/decisions/`: architecture decision records, hardware tradeoffs, and protocol changes.

For the current phase, only the spec file is required. The other folders can be added when planning and execution begin.

## Objective

Build the foundational codebase for a PlatformIO monorepo centered on a 3.5-inch dashboard board first, while keeping a secondary round display board as a later integration target.

1. A 3.5-inch dashboard Master board will own the main system bring-up, local UI, PSRAM validation, and future OBD2 Bluetooth Classic integration.
2. A circular display board will remain a secondary companion display target, informed by the existing GC9A01-based reference project.

This phase establishes the project structure, verifies the primary hardware, and keeps multi-board interaction out of the critical path.

## Product Direction

The product should feel minimalist, futuristic, and intentional rather than crowded with widgets.

The architecture should separate:

1. A primary dashboard that owns data, system state, and driver-critical information
2. A secondary companion display that focuses on animation, mood, and glanceable smart reactions

After hardware verification, UI preview should happen before deeper firmware integration.

## System Overview

### Master Board: `dash_35`

- Hardware: Waveshare ESP32-Touch-LCD-3.5
- MCU: Classic ESP32 with 16MB flash and 2MB PSRAM
- Responsibilities:
  - Initialize display-side platform services and PSRAM
  - Serve as the main compute node for telemetry, state management, and future connectivity
  - Simulate telemetry values in place of real OBD2 input during early bring-up
  - Remain ready for future Bluetooth integration

### Slave Board: `companion_orb`

- Hardware baseline: reference the existing GC9A01-based project at `NitsujY/esp32carconsole`, which uses an ESP32-C3 + round GC9A01 UI architecture
- Responsibilities:
  - Act as a secondary companion display rather than the primary telemetry owner
  - Render local smart-companion visuals, alerts, and assistant states
  - Remain loosely coupled so its transport mechanism can be decided later
  - Reuse proven round-display UI ideas from the reference project where appropriate

## Device Naming Recommendation

The name `companion_orb` is the approved product-oriented name for the secondary display device.

Suggested names:

1. `companion_orb` - recommended
2. `drive_orb`
3. `mood_orb`
4. `pulse_orb`
5. `co_pilot_orb`

Recommended usage:

- Environment or firmware target: `companion_orb`
- Product/UI label: `Orb Companion`

## First-Stage Hardware Focus

The first hardware validation target is the 3.5-inch dashboard board only.

The round display board should not block early progress. It is a second-stage board and can be validated after the main dashboard board is stable.

## Critical Hardware Constraint

The Waveshare Master board uses a classic ESP32 with onboard PSRAM. On this hardware, GPIO 16 and GPIO 17 are reserved by the PSRAM interface and must not be used for `Serial2`.

Because of that, any future external wiring that tries to reuse GPIO 16 and GPIO 17 on the 3.5-inch board must be rejected.

No physical inter-board serial cable is part of the first stage.

## Repository Layout

The foundational repository layout should be:

```text
platformio.ini
include/
  shared_data.h
src/
  dash_35/
    main.cpp
  companion_orb/
    main.cpp
docs/
  specs/
    smart-jdm-dashboard/
      foundation-spec.md
```

## PlatformIO Requirements

The repository must use a PlatformIO monorepo layout with two environments:

1. `[env:dash_35]`
2. `[env:companion_orb]`

### Common Requirements

- Framework: Arduino
- Platform: `espressif32`
- Shared headers provided from `include/`
- Each environment must compile only its own source subtree using `build_src_filter`

### Master Environment Requirements

- Board target: `esp32dev`
- Partition table: `default_16MB.csv`
- PSRAM flags:
  - `-DBOARD_HAS_PSRAM`
  - `-mfix-esp32-psram-cache-issue`
- First-stage firmware should not depend on a physical link to another ESP32

### Slave Environment Requirements

- Board target will remain provisional until the circle companion hardware is frozen
- The reference design is closer to an ESP32-C3 + GC9A01 stack than a generic classic ESP32 dev board

## Feature Staging

### Stage 1: Local Dashboard Bring-Up

- No Wi-Fi
- No physical serial cable between boards
- No dependency on circle board hardware
- Focus on the Waveshare 3.5-inch board only

### Stage 1.5: Pure UI Preview

- Build the UI preview before final embedded UI implementation
- Preview both the dashboard and companion display locally in VS Code
- Use simulated telemetry to drive both previews
- Validate typography, layout, motion, theming, and mode transitions before device UI integration

### Stage 2: OBD2 Connectivity

- Add Bluetooth Classic integration to the 3.5-inch board
- Keep the system standalone and local-first

### Stage 3: Companion Display Integration

- Decide whether the circle board receives data wirelessly or stays semi-autonomous
- Choose the inter-board transport only after radio coexistence is tested on real hardware

### Stage 4: Optional Internet Features

- Wi-Fi becomes a later feature stage only if a real requirement justifies it

## Requested Feature Set

### Welcome Message On Engine Start

- Show a short welcome sequence when engine start is detected
- The sequence should feel premium and cinematic without becoming distracting
- The welcome experience should establish the dashboard personality immediately

### Trip Information And Driving Improvement

The dashboard should collect trip data that can later help improve driving behavior.

Requested examples:

- less sudden acceleration
- fuel saving advice

Recommended additional coaching outputs:

1. harsh acceleration count
2. harsh braking count
3. idle-time estimate
4. smoothness score for the current trip
5. speed consistency score
6. warm-up guidance after cold engine start
7. cooldown reminder after aggressive driving
8. economy-driving shift suggestion
9. estimated fuel-efficiency trend
10. eco-driving summary at trip end

### Sport Mode

- Sport mode activates when speed is greater than 80
- The transformation should feel like a power-up event
- Use stronger accents, more assertive motion, and more dramatic animation states
- Sport mode should affect both the dashboard and the orb companion behavior

### Car Health Dashboard

The information architecture should include a dedicated car-health view.

Recommended initial health widgets:

1. coolant temperature
2. battery voltage
3. fuel level
4. OBD connection status
5. engine-load placeholder for later integration
6. diagnostic summary placeholder for a future stage

## Telemetry Freshness Requirements

Not all dashboard values need the same update cadence.

Driver-critical motion values should refresh more frequently than slow-changing health or estimate values.

Fast-refresh values:

1. `speed`
2. `rpm`

Fast-refresh target:

1. Update every 500 ms to 1000 ms
2. Prefer the lower end of the range when the transport and rendering path can sustain it cleanly

Slow-refresh values:

1. `fuel`
2. `range`
3. `coolant_temp`
4. `battery_voltage`
5. other health or derived estimate values that do not materially change second to second

Slow-refresh target:

1. Update about every 10 seconds
2. Reuse the last known value between refreshes rather than forcing high-frequency polling

Practical requirement:

1. The architecture should support mixed telemetry cadences.
2. Transport and parser logic must not assume every field is refreshed every cycle.
3. UI rendering should prioritize freshness for `speed` and `rpm` while treating `fuel`, `range`, and similar values as coarse-grained status data.

### G-Force Meter

Assumption: your note "reverse room for g-force meter" means "reserve room for g-force meter."

Recommendation:

- Yes, reserve room for it now in the UI architecture.
- Do not force implementation in the first slice unless an IMU or a validated derived-signal approach is committed.

## UI Design Direction

Design priority: `專注於設計極簡且充滿未來感的 UI`

Practical meaning:

- fewer but stronger visual elements
- large glanceable typography for driving safety
- disciplined color usage
- clear hierarchy between critical and secondary data
- motion that communicates state, not decoration for its own sake
- calm normal mode versus intense sport mode

## UI Theme Architecture

Recommended folder structure for a theme-driven dashboard system:

```text
include/
  shared/
    telemetry/
      telemetry_types.h
      trip_metrics.h
src/
  dash_35/
    main.cpp
    app/
      app_state.h
      app_state.cpp
      telemetry_pipeline.h
      telemetry_pipeline.cpp
    ui/
      screens/
        boot_screen.cpp
        welcome_screen.cpp
        dashboard_screen.cpp
        trip_screen.cpp
        health_screen.cpp
        sport_screen.cpp
      widgets/
        speed_card.cpp
        rpm_arc.cpp
        trip_summary.cpp
        health_tile.cpp
        coaching_card.cpp
      animation/
        transitions.cpp
        welcome_fx.cpp
        sport_mode_fx.cpp
      themes/
        theme_types.h
        theme_manager.cpp
        minimalist_future/
          palette.h
          typography.h
          spacing.h
          motion.h
        midnight_sport/
          palette.h
          typography.h
          spacing.h
          motion.h
        neon_circuit/
          palette.h
          typography.h
          spacing.h
          motion.h
  companion_orb/
    main.cpp
    ui/
      screens/
        idle_screen.cpp
        mood_screen.cpp
        alert_screen.cpp
        sport_reaction_screen.cpp
      animation/
        orb_idle_fx.cpp
        orb_reaction_fx.cpp
      themes/
        orb_minimal/
          palette.h
          motion.h
frontend/
  dashboard/
preview/
  companion/
  features/
```

Theme rules:

1. Keep palette, typography, spacing, and motion tokens separate.
2. Avoid hardcoded colors inside screen logic.
3. Let sport mode override a base theme rather than becoming a disconnected UI system.

Recommended theme set:

1. `minimalist_future` as default
2. `midnight_sport` for power mode
3. `neon_circuit` as the most expressive alternate theme

## Local UI Preview Strategy

The preferred pre-flash workflow is a host-side simulator opened from VS Code on macOS.

Recommendation:

1. Build a dashboard preview target for the 320x480 UI
2. Build a companion preview target for the 240x240 round UI
3. Drive both with simulated telemetry
4. Review layout and motion before firmware display integration

Recommended technology:

1. LVGL plus SDL2 simulator for the dashboard preview
2. LVGL plus SDL2 simulator for the orb companion preview

Why this is the correct stage:

- fast visual iteration
- avoids repeated flashing for design changes
- allows theme comparisons inside VS Code
- lets you validate the round mask and composition early

## Shared Telemetry Contract

The shared header must define a `CarTelemetry` struct with the following fields:

- `rpm` (`int`)
- `speed` (`int`)
- `coolant_temp` (`float`)
- `ai_mood` (`int`)
- integrity field for reliable UART transmission

### Integrity Strategy

The initial design should use:

- A simple local telemetry payload shape that can be reused later for either in-process UI updates or inter-board transport
- A lightweight integrity strategy only when a transport is selected

The payload remains useful even before a transport is chosen, because the main dashboard board can use the same structure internally for UI simulation and state updates.

### Serialization Requirements

- The transport must be deterministic and fixed-size.
- The payload must not require heap allocation.
- The layout must remain compatible on both ESP32 devices.
- The implementation should avoid hidden padding issues by using a predictable layout.

## Master Firmware Requirements

The Master firmware file at `src/dash_35/main.cpp` must:

### Setup Phase

- Initialize `Serial` at `115200`
- Verify PSRAM availability and print detected size
- Print enough startup diagnostics to confirm configuration
- Keep hardware bring-up self-contained on the 3.5-inch board

### Loop Phase

- Use a non-blocking `millis()` timer
- Fire telemetry updates every `100 ms`
- Simulate fake RPM values with bounded fluctuation
- Simulate fake speed values with bounded fluctuation
- Populate the shared telemetry struct
- Feed the telemetry into local debug output and later local UI hooks

### Behavioral Constraints

- Must not use `delay()`
- Must not block waiting for peripherals
- Must be structured so real OBD2 Bluetooth logic can replace the simulator later without rewriting the transport path

## Circle Companion Requirements

The circle board is not part of the first implementation gate.

For now, its design assumptions are:

- It is a secondary UI surface, not the main telemetry controller
- It should receive either summarized state or presentation-oriented events later
- It should not own the expensive connectivity stack in the first version
- It can host local animation and assistant-state rendering logic

## UI Preview Requirements

The preview stage should support both device concepts before real UI firmware integration.

Minimum requirements:

1. Dashboard preview sized for 320x480 behavior
2. Companion preview sized for 240x240 round behavior
3. Simulated telemetry playback
4. Theme switching
5. Welcome-sequence preview
6. Sport-mode preview
7. Trip and health-screen preview

## Non-Blocking Design Rules

Both firmware targets must follow these rules:

- No `delay()` in the main loop
- No assumed inter-device blocking dependencies in the first stage
- No dynamic allocation in the hot path
- No direct coupling to UI libraries in this phase
- Clear extension points for LVGL or TFT rendering in later phases

## Telemetry Simulation Rules

During this phase, the Master must simulate telemetry with plausible automotive ranges:

- RPM: idle-to-driving range with smooth variation
- Speed: low-to-moderate driving values with bounded variation
- Coolant temperature: realistic warm-engine range with small drift
- AI mood: integer state derived from driving behavior or a simple cycling model

The simulation must be deterministic enough for parser testing while still visibly changing over time.

## Acceptance Criteria

The foundation is complete when all of the following are true:

1. `dash_35` hardware is verified on real hardware.
2. A local preview exists for the dashboard UI.
3. A local preview exists for the companion display UI.
4. The welcome, trip, health, and sport-mode UI concepts are visible in preview before firmware UI integration.
5. The codebase is ready for the next implementation stage after design approval.

## Out of Scope for This Phase

The following are intentionally excluded from the foundation implementation:

- Real ELM327 Bluetooth Classic integration
- Wi-Fi hotspot connection logic
- Inter-board wireless transport
- Physical serial wiring between boards
- TFT_eSPI or GC9A01 rendering code
- AI animation engine
- OTA update flow
- Persistent storage or settings management
- Production telemetry protocol versioning

The local UI preview simulator is not out of scope. It is a required stage after hardware verification.

## Next Planned Deliverables

After this spec is implemented, the natural next documents are:

1. `docs/plans/smart-jdm-dashboard/foundation-plan.md`
2. `docs/tasks/smart-jdm-dashboard/foundation-tasks.md`
3. Decision records under `docs/decisions/` for UART framing, pin mapping, and display stack selection
