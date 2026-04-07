# Headless Web Release Checklist

This checklist follows the current delivery order: preview first, cleanup second, hotspot reliability third, integrated validation fourth, release last.

## Stage 1: Web Preview Release Candidate

- [ ] Review the desktop browser dashboard for typography, spacing, readability, and motion quality
- [ ] Review the phone browser dashboard for layout stability, tap-target safety, and scrolling behavior
- [ ] Confirm WebSocket reconnect behavior on browser refresh and temporary socket loss
- [ ] Confirm the same dashboard build works with mock data and with a configurable live socket override
- [ ] For each UI change, verify both the preview flow and the live OBD-backed board flow in the same iteration
- [ ] Lock the design direction before further dash_35 cleanup starts

## Stage 2: Cleanup And Simplification

- [ ] Remove unused dash_35 display sources from the active repository path once design is approved
- [ ] Remove or retire unused dash_35 runtime features that are no longer in the release scope
- [ ] Keep the active dash_35 code path limited to telemetry, Wi-Fi, mDNS, web serving, and WebSocket broadcast
- [ ] Rebuild dash_35 after each cleanup step

## Stage 3: iPhone Hotspot Reliability

- [ ] Boot the ESP32 with the iPhone hotspot already enabled and confirm connection
- [ ] Boot the ESP32 before enabling the hotspot and confirm eventual connection
- [ ] Toggle the hotspot OFF and ON while the ESP32 is running and confirm reconnect
- [ ] Power-cycle the ESP32 repeatedly and confirm reconnect across several runs
- [ ] Record direct-IP access success separately from mDNS success
- [ ] If `carconsole.local` is inconsistent on the hotspot, document it as a hotspot limitation and keep direct IP as the fallback path

## Stage 4: Live dash_35 And Phone Behavior

- [ ] Connect the phone browser to the real dash_35 WebSocket endpoint
- [ ] Confirm live OBD values render cleanly and update smoothly
- [ ] Confirm stale-data indication appears when telemetry pauses
- [ ] Confirm reconnect behavior after board reset and after page reload
- [ ] Confirm the phone layout remains responsive during real telemetry updates

## Stage 5: Release

- [ ] Run `npm run build:web`
- [ ] Run `pio run -e dash_35`
- [ ] Run `pio run -e dash_35 -t buildfs`
- [ ] Upload the LittleFS image
- [ ] Upload the firmware image
- [ ] Verify access by direct IP
- [ ] Verify access by `carconsole.local` where supported
- [ ] Capture final release notes for the preferred path and the fallback path
