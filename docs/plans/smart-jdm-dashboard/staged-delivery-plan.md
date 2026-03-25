# Smart JDM Dashboard Staged Delivery Plan

## Purpose

This document separates pre-implementation validation from firmware coding so hardware and protocol risks are discovered early.

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

Why it exists:

- Prevent UI and transport work from being coupled too early
- Allow parser and transport behavior to be tested in isolation

### Stage 5: Firmware Implementation

Outputs:

- Working `platformio.ini`
- Shared transport header
- Master firmware
- Slave firmware

Why it exists:

- Implementation starts only after the environment and hardware are known-good

## Gate Policy

The start of Stage 5 requires explicit user approval.
