# Hardware Verification Checklist

## Purpose

Use this checklist before firmware implementation proceeds.

## Physical Checks

- [x] `dash_35` powers on reliably
- [x] USB cable supports data, not only power
- [x] LCD shows a stable image or factory/demo content
- [x] Touch panel is responsive in a known-good demo

## Host Connectivity Checks

- [x] `dash_35` serial port appears on the host computer
- [x] `dash_35` can open a serial monitor at 115200 baud

## `dash_35` Checks

- [x] Board boots without repeated resets
- [x] PSRAM is detected successfully
- [x] No obvious brownout or boot-loop behavior is observed

## Approval Condition

Implementation should begin only after the `dash_35` checks above are confirmed.

## USB Port Assignments

| Board           | USB Port                        |
|-----------------|---------------------------------|
| `dash_35`       | `/dev/cu.usbmodem5AAF1861091`   |
| `companion_orb` | `/dev/cu.usbmodem114401`        |

Note: The `companion_orb` is an ESP32-C3. The RISC-V toolchain requires Rosetta 2 on Apple Silicon.
