# Documentation Layout

This repository uses a lightweight spec-driven development structure.

## Folders

- `docs/specs/`: stable product and system requirements
- `docs/plans/`: implementation sequencing and delivery strategy
- `docs/tasks/`: execution-level work breakdown
- `docs/decisions/`: architecture decision records and hardware tradeoffs

## Current Documents

- `docs/specs/smart-jdm-dashboard/foundation-spec.md`
- `docs/specs/smart-jdm-dashboard/uart-protocol-interaction.md`
- `docs/specs/smart-jdm-dashboard/persistent-trip-data.md`
- `docs/specs/smart-jdm-dashboard/touch-screen-navigation.md`
- `docs/specs/smart-jdm-dashboard/wifi-hotspot-weather.md`
- `docs/specs/smart-jdm-dashboard/live-traffic-overlay.md`
- `docs/specs/smart-jdm-dashboard/fuel-price-lookup.md`
- `docs/specs/smart-jdm-dashboard/speed-camera-alerts.md`
- `docs/specs/smart-jdm-dashboard/ntp-time-sync.md`
- `docs/plans/smart-jdm-dashboard/foundation-plan.md`
- `docs/plans/smart-jdm-dashboard/staged-delivery-plan.md`
- `docs/tasks/smart-jdm-dashboard/foundation-tasks.md`
- `docs/tasks/smart-jdm-dashboard/hardware-verification-checklist.md`
- `docs/decisions/smart-jdm-dashboard/stage-4-protocol-approval.md`
- `docs/decisions/smart-jdm-dashboard/stage-5-display-smoke-test-stack.md`

## Current Stage

- Stage 4: interaction validation complete
- Stage 5: implementation active

## Device Architecture (Refactored)

| Device | Role | Connection |
| ------ | ---- | ---------- |
| ESP32-C3 `companion_orb` | **Primary in-car display** (OBD2 only) | Direct UART → ELM327 OBD2 |
| iPhone 15 (optional) | Secondary dashboard / data logging | *Decoupled, not used in orb-only mode* |
| ESP32 `dash_35` | Deprecated (reference only) | N/A |

The **Orb is now standalone**: it connects directly to the OBD2 adapter via UART, displays speed/RPM with tamagotchi UI, and needs no external relay. This eliminates the complexity of WiFi + ESP-NOW broadcasting, which was causing data loss.

**See [REFACTOR.md](/REFACTOR.md) for full migration details.**

Deferred later stage:

- master/slave framed transport hookup
- `companion_orb` board integration

## Usage

1. Write or update the spec first.
2. Create a staged plan with explicit approval gates.
3. Install or verify the local toolchain.
4. Verify hardware before board-to-board firmware work.
5. Build and review the pure UI preview before embedded UI implementation.
6. Approve protocol behavior before implementation begins.
7. Implement code against the task list.
8. Record notable tradeoffs in `docs/decisions/` when the architecture changes.
9. On `dash_35` hardware, press BOOT after startup to toggle local demo telemetry for web UI testing; do not hold BOOT during reset or power-on because GPIO 0 is still the ESP32 flash-mode strap.
