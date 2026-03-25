# Companion Interaction Strategy

## Purpose

This document isolates the future board-to-board interaction contract from the rest of the application so transport choices do not block the first hardware stage.

## Actors

### Master: `dash_35`

- Produces telemetry and owns the main application state
- Owns future vehicle connectivity and data normalization
- Will later replace simulated telemetry with real OBD2-derived values

### Slave: `companion_orb`

- Acts as a secondary smart display and local animation surface
- Should consume a reduced presentation-oriented state rather than raw full-system ownership
- Will later map validated state into display and animation logic

## First-Stage Rule

No physical cable is required between boards in the first stage.

## Recommended Interaction Model

### Stage 1

- `dash_35` runs standalone
- `companion_orb` is not required for bring-up
- No inter-board transport is selected yet

### Stage 2

- `dash_35` adds OBD2 Bluetooth Classic
- `companion_orb` remains optional and secondary

### Stage 3

- A pure UI-preview phase validates both dashboard and orb behavior locally before transport is chosen

### Stage 4

- Select a wireless link only after coexistence testing
- Candidate options may include ESP-NOW or a BLE-based side channel
- Wi-Fi should remain deferred unless a specific feature needs internet access

## State Contract

The shared state shape is still useful even before transport is chosen. It should contain:

1. `rpm`
2. `speed`
3. `coolant_temp`
4. `ai_mood`

The transport framing details are intentionally deferred.

## Interaction Sequence

### Nominal Flow

1. `dash_35` boots and validates its local hardware.
2. `dash_35` generates or acquires telemetry.
3. `dash_35` derives a reduced companion state for the round display.
4. `companion_orb`, when introduced, renders assistant visuals from that reduced state.

### Smart Companion Behavior

The circle companion should focus on UI-facing behavior such as:

1. Mood and persona changes based on speed, rpm, or driving intensity
2. Friendly status prompts such as warm-up, calm cruise, or spirited driving
3. Shift-warning or caution animations
4. Boot and idle animation states
5. Lightweight trip or notification widgets

These are local features and do not require internet access.

## Internet Requirement

Internet is not required for the smart companion concept if it is implemented as a local rule-based assistant.

Internet is only needed for later cloud-backed features such as:

1. Remote LLM inference
2. Weather
3. Online navigation data
4. OTA updates

## Protocol Review Questions

These items should be explicitly approved before implementation resumes:

1. Is `dash_35` standalone hardware bring-up the correct first gate?
2. Is a pure UI-preview stage the correct next step after hardware verification?
3. Is Wi-Fi correctly deferred to a later feature stage?
4. Should `companion_orb` stay secondary and display-oriented rather than connectivity-oriented?
5. Should inter-board transport be postponed until radio coexistence is tested on the real hardware?

## Approval Gate

Implementation should proceed only after the user approves this staged interaction model and the hardware verification checklist for `dash_35` has been completed.
