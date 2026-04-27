# OBD2 Probe (macOS)

Small Python tool for probing a BLE ELM327 adapter from your Mac. This keeps raw adapter and ECU debugging off the ESP32 while you figure out the exact commands your car responds to.

## Setup

From the repo root:

```bash
cd tools/obd2_probe
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

macOS may ask you to grant Bluetooth access to the terminal app you are using.

## Usage

Scan for nearby BLE devices:

```bash
python -m obd2_probe scan
```

Inspect services and characteristics:

```bash
python -m obd2_probe services --address <BLE_ID>
```

Open an interactive REPL:

```bash
python -m obd2_probe repl --address <BLE_ID>
```

Run an automated scan-and-test pass:

```bash
python -m obd2_probe test --name IOS-Vlink \
  --tx-uuid 00002af1-0000-1000-8000-00805f9b34fb \
  --rx-uuid 00002af0-0000-1000-8000-00805f9b34fb \
  --include-toyota-fuel
```

Scan Toyota custom Mode 21 or 22 PID ranges:

```bash
python -m obd2_probe toyota-scan --name IOS-Vlink \
  --tx-uuid 00002af1-0000-1000-8000-00805f9b34fb \
  --rx-uuid 00002af0-0000-1000-8000-00805f9b34fb \
  --header 7C0 --mode 21 --pid-start 0x00 --pid-end 0xFF
```

Inside the REPL:

- Type ELM commands like `ATI`, `ATZ`, `ATE0`, `ATSP6`, `0100`, `010C`, `010D`, `2129`
- Use `:init` for a common safe startup sequence
- Use `:quit` to exit

## Less Typing

For repeated runs, set environment variables in your shell:

```bash
export OBD2_BLE_ID="<BLE_ID>"
export OBD2_TX_UUID="<BLE_TX_UUID>"
export OBD2_RX_UUID="<BLE_RX_UUID>"
```

Then run:

```bash
python -m obd2_probe services --address "$OBD2_BLE_ID"
python -m obd2_probe repl --address "$OBD2_BLE_ID" \
  --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" \
  --resp-timeout 20
python -m obd2_probe test --address "$OBD2_BLE_ID" \
  --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" \
  --include-toyota-fuel
```

The `test` command will:

- scan by name if you do not pass `--address`
- connect and enable notifications
- run init plus `ATI`, `ATDP`, `0100`, `010C`, and `010D`
- optionally run the Toyota `2129` fuel sequence
- exit non-zero if the expected replies are not seen

The `toyota-scan` command will:

- initialize the adapter and set the requested protocol and CAN header
- sweep a Toyota diagnostic mode across a PID range
- print only the PIDs that return payloads instead of silence or `NO DATA`
- give you a quick way to compare the car in two states, such as headlights OFF vs ON

## Fuel Checks For Toyota Sienta

The firmware now uses the Toyota-specific fuel path directly for the active Sienta profile:

```text
:init
ATSP6
ATH1
ATSH7C0
2129
```

Expected response pattern:

```text
7C8 03 61 29 XX
```

Where `XX` is a single data byte. The current firmware interprets this as:

$$
fuelLiters = \frac{XX}{2} + offsetLiters
$$

$$
fuelPercent = \frac{fuelLiters}{tankLiters} \times 100
$$

For the active Toyota Sienta profile:

- `tankLiters = 42`
- `offsetLiters` comes from `toyota_fuel_offset_liters` in the vehicle profile
- `fuel_scale_percent` is applied after the percent conversion for final calibration

If you want to capture more evidence before adjusting the offset, also run:

```text
ATDP
0100
0120
0103
0106
0107
0144
```

That gives you the protocol confirmation plus the same raw fuel-related checks documented in your earlier DIY probe flow.

## Toyota Custom PID Discovery Workflow

For body-state signals like headlights, doors, or HVAC toggles, use a differential scan instead of guessing one PID at a time.

Recommended workflow:

1. Keep the car in one stable state, such as ignition ON and headlights OFF.
2. Run `toyota-scan` over one header and one mode, for example header `7C0` with Mode `21`.
3. Save the responding PID list and payload bytes.
4. Change exactly one car state, such as turning the headlights ON.
5. Repeat the exact same scan.
6. Diff the two outputs and focus on PIDs whose payload bytes changed only with that one switch action.
7. Repeat on headers `750` and `7E0`, then repeat with Mode `22` if Mode `21` does not expose the signal.

Suggested scan matrix for Toyota Sienta:

```bash
python -m obd2_probe toyota-scan --address "$OBD2_BLE_ID" --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" --header 7C0 --mode 21 --pid-start 0x00 --pid-end 0xFF
python -m obd2_probe toyota-scan --address "$OBD2_BLE_ID" --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" --header 750 --mode 21 --pid-start 0x00 --pid-end 0xFF
python -m obd2_probe toyota-scan --address "$OBD2_BLE_ID" --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" --header 7E0 --mode 21 --pid-start 0x00 --pid-end 0xFF
python -m obd2_probe toyota-scan --address "$OBD2_BLE_ID" --tx-uuid "$OBD2_TX_UUID" --rx-uuid "$OBD2_RX_UUID" --header 7C0 --mode 22 --pid-start 0x0000 --pid-end 0x00FF
```

Practical advice:

- Change only one physical switch per scan pass.
- Do at least two OFF/ON/OFF cycles before trusting a candidate PID.
- Prefer stable bytes over counters or timestamps.
- Once a candidate looks promising, confirm it in the REPL with repeated manual reads before wiring it into firmware.

## Defaults

The probe prefers Nordic UART Service UUIDs by default:

- Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- TX: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- RX: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

If your adapter uses different UUIDs, pass them explicitly to `services` or `repl`.
