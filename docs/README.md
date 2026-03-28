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
- `docs/plans/smart-jdm-dashboard/foundation-plan.md`
- `docs/plans/smart-jdm-dashboard/staged-delivery-plan.md`
- `docs/tasks/smart-jdm-dashboard/foundation-tasks.md`
- `docs/tasks/smart-jdm-dashboard/hardware-verification-checklist.md`
- `docs/decisions/smart-jdm-dashboard/stage-4-protocol-approval.md`
- `docs/decisions/smart-jdm-dashboard/stage-5-display-smoke-test-stack.md`

## Current Stage

- Stage 4: interaction validation complete
- Stage 5: implementation active

Current Stage 5 slice:

- `dash_35`-first standalone implementation
- dashboard-screen selection and trip/coaching state pipeline
- ST7796 display smoke-test rendering on `dash_35`
- companion linkage intentionally deferred

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
