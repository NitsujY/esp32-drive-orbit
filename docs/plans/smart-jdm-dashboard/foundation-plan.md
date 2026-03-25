# Smart JDM Dashboard Foundation Plan

## Status

- State: Pre-implementation gate
- Date: 2026-03-24
- Spec: `docs/specs/smart-jdm-dashboard/foundation-spec.md`
- Delivery model: staged, approval-gated

## Goal

Reduce implementation risk by moving in explicit stages:

1. Freeze the design and workflow documents
2. Confirm local toolchain readiness
3. Verify hardware health before firmware coupling
4. Verify UART protocol assumptions independently of the final UI stack
5. Start implementation only after approval

## Stage Breakdown

### Stage 0: Documentation Freeze

Deliverables:

1. Foundation specification
2. Staged delivery plan
3. Hardware verification checklist
4. Execution task list

Exit criteria:

1. Repository contains durable docs under `docs/`
2. Wiring decisions and constraints are written down
3. The implementation sequence is split into explicit approval gates

### Stage 1: Toolchain Readiness

Deliverables:

1. PlatformIO platform packages installed locally
2. Arduino ESP32 framework available locally
3. Toolchain path confirmed in the environment

Exit criteria:

1. `espressif32` platform is installed
2. ESP32 Arduino framework is installed
3. The repo is ready for later compile validation without additional package downloads

### Stage 2: Hardware Verification

Purpose:

Verify both boards behave correctly in isolation before testing any cross-board firmware interactions.

Checks:

1. Master powers reliably over USB
2. Slave powers reliably over USB
3. Both boards enumerate as serial devices
4. Master PSRAM is detected at boot
5. Slave serial console is stable
6. Remapped UART pins are wired as designed
7. Common ground is present between boards

Exit criteria:

1. User confirms both boards boot and remain stable
2. User confirms UART wiring uses Master GPIO 25/26 and Slave GPIO 16/17
3. User confirms no hardware anomalies before protocol testing begins

### Stage 3: Protocol and Interaction Validation

Purpose:

Validate the transport contract and board-to-board interaction before adding larger application behavior.

Checks:

1. Packet shape is fixed and documented
2. Checksum behavior is documented and testable
3. Slave parser requirements are defined for partial packets and resynchronization
4. Interaction sequence between Master transmit and Slave receive is documented
5. Mixed telemetry cadence is documented so fast motion data and slow status data are not treated identically

Exit criteria:

1. Protocol contract is approved
2. Interaction behavior is approved
3. Implementation is explicitly authorized

### Stage 4: Implementation

This stage is intentionally blocked until the first three stages are complete and approved.

Planned scope after approval:

1. `platformio.ini`
2. `include/shared_data.h`
3. `src/dash_35/main.cpp`
4. `src/companion_orb/main.cpp`

## Risks

- Master UART pins must not use GPIO 16 or GPIO 17 due to PSRAM reservation
- Physical hardware validation cannot be executed remotely from this environment
- Raw binary transport is only safe while both endpoints remain ESP32/little-endian
- Display pin usage may constrain future peripheral routing on the Waveshare board

## Approval Gate

Do not proceed with further implementation work until:

1. Toolchain installation is complete
2. Hardware verification has been performed by the user
3. The user approves the implementation stage
