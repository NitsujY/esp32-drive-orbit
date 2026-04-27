# Smart JDM Dashboard Staged Delivery Plan

## Purpose

This plan is now ordered around a browser-first release flow.

The web preview is the primary product until the design is locked on desktop and phone. Only after that is stable do we aggressively remove leftover code, harden hotspot reliability, validate the end-to-end dash_35 (legacy) plus phone behavior, and ship the final ESP32-hosted release.

## Current Direction

- `dash_35` (legacy) is being reduced to a headless telemetry gateway.
- The browser dashboard is the main UI surface and should be finalized before deeper firmware cleanup continues.
- `companion_orb` is not part of the active delivery path for this release.
- The immediate priority is not feature expansion. The priority is sequencing: preview release first, cleanup second, hotspot reliability third, integrated behavior validation fourth, release last.
- Every dashboard-facing change must be validated in both the preview flow and the live OBD-backed board flow in the same iteration so the preview does not drift from real behavior.

## Delivery Order

### Stage 0: Planning Reset

Outputs:

- A repo plan that matches the browser-first delivery order
- A concrete validation checklist for preview, hotspot, and release gates
- Explicit de-scoping of companion work from the current release

Why it exists:

- The old plan assumed a display-centric and inter-board path that is no longer the shipping direction
- The repo needs one source of truth for the new release order

### Stage 1: Web Preview Release Candidate

Outputs:

- A stable local browser dashboard that runs from the Mac
- A mock WebSocket feed for instant UI iteration
- A phone-accessible preview flow over LAN or tunnel
- A design review gate focused on layout, motion, readability, and reconnect behavior

Why it exists:

- Design changes are cheapest before firmware cleanup and hotspot hardening consume time
- The browser UI is now the shipping experience, so it must be treated as the first release candidate

Acceptance gate:

- Desktop browser preview feels visually stable and coherent
- Phone browser preview is usable and responsive on the same UI build
- Preview changes are checked against the live OBD-backed dashboard path in the same working cycle
- The team can sign off on the visual direction before firmware cleanup starts

### Stage 2: Code Cleanup And Runtime Simplification

Outputs:

- `dash_35` (legacy) reduced to only the code required for OBD polling, Wi-Fi, mDNS, AsyncWebServer, WebSocket broadcast, and LittleFS serving
- Old dash_35 (legacy) display code removed from the repository or clearly retired after a final review
- No weather, camera, ESP-NOW, or local rendering logic left in the active dash_35 (legacy) path

Why it exists:

- Cleanup should happen after the web preview direction is approved, not before
- The firmware must stay clean and understandable while hotspot and integration testing proceed

Acceptance gate:

- The dash_35 (legacy) source tree is easy to read and limited to the gateway responsibilities
- `pio run -e dash_35` stays green after cleanup

### Stage 3: iPhone Hotspot Reliability Validation

Outputs:

- A repeatable Wi-Fi validation procedure against the iPhone hotspot
- Reliable reconnect behavior across power cycles and hotspot restarts
- Logs and fallback notes for cases where mDNS is not dependable on the hotspot

Why it exists:

- The release is blocked if the ESP32 cannot join the iPhone hotspot consistently
- This is a system behavior problem, not just a code-completion step

Acceptance gate:

- The ESP32 connects successfully across repeated attempts, not just once
- The dashboard remains reachable by direct IP even if `carconsole.local` is inconsistent
- Failures, if any, are reduced to a small documented set of known hotspot limitations

### Stage 4: End-To-End dash_35 (legacy) And Phone App Behavior Test

Outputs:

- Verified behavior with live `dash_35` (legacy) telemetry feeding the browser dashboard
- Confirmed reconnect behavior when the board restarts, the hotspot cycles, or the browser reloads
- Confirmed UI responsiveness, stale-data behavior, and visual stability on the phone

Why it exists:

- A good local preview is not enough; the integrated board-plus-phone path must behave correctly under real conditions
- This stage catches the gap between mock telemetry and live OBD timing

Acceptance gate:

- The phone app stays readable and responsive with live board data
- Live telemetry recovery after disconnects works without manual repair steps beyond page refresh or built-in reconnect
- The system is stable enough to move to release packaging

### Stage 5: Release Build And Deployment

Outputs:

- Minified and gzipped frontend assets
- A buildable firmware image and LittleFS image
- A repeatable release sequence for firmware upload and filesystem upload
- Release notes describing the preferred access path and fallback access path

Why it exists:

- Packaging needs to happen only after preview approval, cleanup, hotspot reliability, and integrated behavior validation are complete

Acceptance gate:

- `npm run build:web` succeeds
- `pio run -e dash_35` succeeds
- `pio run -e dash_35 -t buildfs` succeeds
- Firmware and filesystem upload steps are documented and reproducible

## Immediate Next Steps

1. Treat the current browser dashboard as a release candidate and iterate only on design-quality issues until the preview is approved.
2. Every preview-facing UI update must be verified against the live OBD path before the iteration is considered complete.
3. After preview approval, physically remove unused dash_35 (legacy) code instead of only excluding it from the build.
4. Run repeated iPhone hotspot connection trials and tighten Wi-Fi behavior until it is dependable.
5. Validate live dash_35 (legacy) plus phone behavior with the real board, not the mock feed.
6. Only then produce and deploy the final release assets.

## Explicit Non-Goals For This Release

- No new companion_orb delivery work
- No reintroduction of local TFT or LVGL rendering
- No expansion into extra non-essential runtime features before hotspot reliability is proven
