# Smart JDM Dashboard Foundation Tasks

## Stage 0: Documentation

- [x] Create persistent design specification in the repository
- [x] Create documentation structure for specs, plans, and tasks
- [x] Add staged delivery plan
- [x] Add hardware verification checklist

## Stage 1: Toolchain Readiness

- [x] Install PlatformIO Espressif32 platform packages
- [x] Confirm Arduino ESP32 framework is installed locally
- [x] Confirm toolchain is ready for later compile validation

## Stage 2: Hardware Verification

- [x] Verify `dash_35` powers and enumerates over USB
- [x] Verify `dash_35` display and touch baseline
- [x] Verify `dash_35` PSRAM is detected on hardware
- [x] Confirm `dash_35` is stable before protocol work

## Stage 3: Pure UI Preview

- [x] Choose preview technology for both device concepts
- [x] Create dashboard preview target
- [x] Create companion preview target
- [x] Preview welcome sequence
- [x] Preview trip and health views
- [x] Preview sport mode transformation
- [x] Review theme architecture and approve default theme set

## Stage 4: Protocol Approval

- [x] Review local-first architecture split
- [x] Approve no-Wi-Fi first-stage scope
- [x] Approve deferred inter-board transport decision
- [x] Approve `companion_orb` as a secondary display-oriented board

## Stage 5: Implementation

- [x] Add PlatformIO multi-environment configuration
- [x] Add shared telemetry packet definition
- [x] Implement Master simulated telemetry publisher
- [x] Build and verify `dash_35`
- [x] Add `dash_35` standalone dashboard-state and trip/coaching pipeline
- [x] Prioritize `dash_35` standalone embedded UI integration
- [x] Add `dash_35` display smoke test using Waveshare ST7796 bring-up
- [x] Add boot and welcome screen state to `dash_35`
- [x] Integrate ELM327 Bluetooth Classic transport on the Master
- [x] Add explicit pre-connection, connecting, and live OBD status UX on `dash_35`
- [x] Validate Toyota Sienta adapter targeting and PID reads on hardware
- [x] Replace simulated telemetry as the primary source once live vehicle data is verified
- [x] Resolve `Orbital Driver View` overlap with the bottom toolbar on the embedded layout

## Stage 6: Companion Linkage And Integration

- [x] Implement Slave UART parser with partial-packet handling
- [x] Add framed fast/status telemetry transmitter on the Master
- [x] Build and verify `companion_orb`
- [x] Connect the framed transport to real inter-board I/O
- [x] Add GC9A01 rendering on the Slave
- [ ] Validate live `dash_35` to `companion_orb` telemetry exchange

## Follow-Up Tasks

- [ ] Add Wi-Fi/mobile hotspot support on the Master
- [ ] Add full embedded UI framework decision for post-smoke-test rendering
