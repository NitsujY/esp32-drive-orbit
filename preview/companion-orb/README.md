# Orb — Independent OBD2 Display

Interactive web preview of the **Orb** device - now the single device in the car, connecting directly to OBD2.

## Architecture

- **Device**: ESP32-C3 (companion_orb)
- **Connection**: Direct UART → ELM327 OBD2 adapter (no ESP-NOW relay)
- **Display**: Small LCD showing speed, RPM, and pet tamagotchi
- **Telemetry**: Speed, RPM (simplified from complex multi-device setup)

## Web Preview

Open `preview/companion-orb/index.html` in your browser to preview the orb UI.

Recommended (serve locally):

```bash
# from repository root
python3 -m http.server 8123 --directory preview/companion-orb
# then open http://localhost:8123
```

Controls simulate real telemetry:
- **Speed** (0–200 km/h) — displayed large on overlay
- **RPM** (0–7000) — displayed and shown in bottom bar
- **Fuel Level** — tracked for pet behavior
- **Pet Mood** — responds to drive patterns (neutral, warm, alert, happy, sad, excited)
- **Drive Mode** — chill (blue) vs sport (orange) theme
- **Pet XP & Sleep** — tamagotchi-style progression

## Firmware

See `src/companion_orb/` for the embedded firmware code. The orb:
1. Reads OBD2 PIDs directly via UART (ELM327)
2. Drives a small LCD display with the UI
3. Maintains pet state based on driving behavior

