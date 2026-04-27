# Toyota Headlight PID Research

**Adapter:** IOS-Vlink (`64543134-6384-D872-24F2-4B44EE306A63`)
**Vehicle:** Toyota Sienta (or similar modern Toyota with CAN bus)
**ECU Header:** `7C0`
**OBD Mode:** `21`

A multi-state toggle sequence for the car's lighting switch was recorded to find accurate bits corresponding to the Headlight ON/OFF state. 

Out of Mode 21's `0x13` to `0xC5` ranges, 18 PIDs responded. Three PIDs showed active value changes during the toggling: **`2113`**, **`2123`**, and **`21C4`**.

## Observed Log Sequences

The physical switch was flipped through 5 sequential states:

1. **State 1 (Orig OFF)**
   - `2113`: `7A` (0111 1010)
   - `2123`: `9E` (1001 1110)
   - `21C4`: `3A` (0011 1010)

2. **State 2 (ON)**
   - `2113`: `76` (0111 0110)  (Bits `8` -> 0, `4` -> 1. Net change: `-4` from `A`)
   - `2123`: `9C` (1001 1100)
   - `21C4`: `38` (0011 1000)

3. **State 3 (New OFF)**
   - `2113`: `79` (0111 1001)
   - `2123`: `99` (1001 1001)
   - `21C4`: `39` (0011 1001)

4. **State 4 (ON)**
   - `2113`: `74` (0111 0100)
   - `2123`: `94` (1001 0100)
   - `21C4`: `36` (0011 0110)

5. **State 5 (Final OFF)**
   - `2113`: `77` (0111 0111)
   - `2123`: `91` (1001 0001)
   - `21C4`: `38` (0011 1000)  <- *Notice `38` equals State 2's ON state. 21C4 is unreliable for pure ON/OFF.*

## Conclusion / Implementation

PID `21C4` showed an identical value (`38`) during an "ON" and an "OFF" state, suggesting it governs an intermediate feature (like DRLs, high-beams vs low-beams, or fog lights) rather than the primary dashboard dimming switch.

PID **`2113`** and **`2123`** are the authoritative indicators. The raw hex bytes walk down a sequence of bit states (`A->6->9->4->7` and `E->C->9->4->1`), which heavily implies a multi-position switch (e.g., Auto / Parking / Low / High) passing through independent states.

The lowest common denominator for "Headlights are visibly throwing light on the road" (States 2 and 4):
- **`2113` Payload**: `0x76` or `0x74` 
- **`2123` Payload**: `0x9C` or `0x94`

In logic processing, a reliable mask to detect these "ON" states against the "OFF" states:
- `(payload2113 & 0x08) == 0` for ON detection. (Both `6` and `4` lack the `8` bit, whereas `A` and `9` have it. Note that `7` lacks it too, so more complex masking may be required if the `7` state is a true "OFF").

*Recommendation: Map `2113` exactly to `{0x76: ON, 0x74: ON}` and treat all other values as OFF to trigger the dashboard's night mode.*