# UI Preview Stage Spec

## Purpose

Validate the dashboard and companion display UI locally before real firmware UI integration and before repeated device flashing becomes part of the workflow.

## Goals

1. Preview the 3.5-inch dashboard UI in a host-side window
2. Preview the round companion UI in a host-side window
3. Simulate vehicle telemetry locally
4. Validate welcome flow, trip view, health view, and sport-mode transition
5. Compare themes quickly inside VS Code

## Recommended Preview Stack

Recommended approach:

1. LVGL + SDL2 for the dashboard preview
2. LVGL + SDL2 for the companion preview

This matches the proven preview approach used in the referenced round-display project and keeps the visual workflow fast.

## Preview Scenarios

### Dashboard Preview

- boot state
- welcome message
- default driving dashboard
- trip summary screen
- car-health screen
- sport mode when speed exceeds 80

### Companion Preview

- idle animation
- welcome reaction
- calm-drive mood
- sport reaction state
- alert state

## Acceptance Criteria

1. Both preview targets run locally on macOS inside VS Code terminals or tasks.
2. Simulated telemetry can trigger screen-state changes.
3. Theme switching is possible without flashing firmware.
4. The round display composition is validated against a circular mask.
5. The user approves the look and feel before embedded UI implementation begins.
