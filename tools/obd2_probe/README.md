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
python -m obd2_probe test --name V-LINK \
  --tx-uuid 00002af1-0000-1000-8000-00805f9b34fb \
  --rx-uuid 00002af0-0000-1000-8000-00805f9b34fb \
  --include-toyota-fuel
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

## Defaults

The probe prefers Nordic UART Service UUIDs by default:

- Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- TX: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- RX: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

If your adapter uses different UUIDs, pass them explicitly to `services` or `repl`.
