# Dashboard Design Concepts — 10 Radical Directions

## Status: Draft — Design Review

## Context

Strip the dashboard to its core: **Speed** and **RPM** as hero elements, with minimal secondary info (fuel level, trip distance, gear indicator). Remove all clutter — no weather panels, no G-meter, no hero pods, no rolling status bars, no multi-card dock rails. The driver glances once and knows everything.

**Target device:** iPhone 15 (Capacitor iOS app) in landscape orientation. Previewed in desktop browser.

**Device architecture:**

| Device | Role | Display |
| ------ | ---- | ------- |
| iPhone 15 (iOS app) | Primary dashboard UI | 852 × 393 CSS pts landscape (2556 × 1179 @3x) |
| ESP32 `dash_35` | Headless telemetry gateway | No local rendering — OBD polling, WiFi, WebSocket broadcast |
| ESP32-C3 `companion_orb` | Ambient companion display | 240 × 240 round GC9A01 (deferred) |

The dashboard is a web app served via Capacitor on the iPhone. The ESP32 `dash_35` acts as a headless gateway only — it does not render any UI. All visual design targets the iPhone screen.

**Data available:** `spd` (km/h), `rpm`, `fuel` (%), `rng` (km), `clt` (°C), `bat` (mV), `acc` (milli-g), `hl`, `obd`, `wifi`, `fresh`.

**Core hierarchy for every concept:**
1. Speed — the single largest, most dominant element
2. RPM — the primary motion/energy indicator beside speed
3. Fuel + Range — small but always visible
4. Trip distance — subtle persistent counter
5. Gear indicator — compact badge (P / D / D3 / D4)

---

## Concept 1 — "ORBITAL ARC"

**Vibe:** Sci-fi cockpit from 2040. A single sweeping 270° arc dominates the screen.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│          ╭━━━━━━━━━━━╮              │
│        ╱   RPM ARC     ╲            │
│      ╱                   ╲          │
│     │     ┌─────────┐     │         │
│     │     │   182    │     │         │
│     │     │  km/h    │     │         │
│     │     └─────────┘     │         │
│      ╲                   ╱          │
│        ╲_______________╱            │
│                                     │
│  [D4]   FUEL 72% ━━━━━━━░░  410km  │
│                    TRIP 34.2 km     │
└─────────────────────────────────────┘
```

**Speed:** Centered inside a huge circular RPM arc. Massive mono-weight numerals (120px+), no decoration. The number itself is the visual anchor.

**RPM:** A thick 270° arc ring wrapping around the speed. Gradient fill from cool blue (idle) → cyan (mid) → hot magenta (redline). Arc thickness ~28px. Tick marks every 1000 RPM as subtle notches on the outer edge. No numeric RPM label — the arc width speaks.

**Fuel:** Single horizontal micro-bar at the bottom with percentage left, range km right. Thin (4px height), barely there until fuel drops below 20% (then it pulses amber).

**Trip:** Tiny right-aligned text below the fuel bar.

**Gear:** Small rounded pill badge, bottom-left corner, semi-transparent.

**Colors:** Deep navy (#05091a) background. Speed text in pure white. RPM arc uses a gradient: `#4facfe` → `#00f2fe` → `#f953c6`. Fuel bar in muted teal. Ambient glow radiates from center outward.

**Typography:** Custom geometric sans-serif (think Eurostile / Orbitron family). Speed digits are ultra-thin and tall. Unit label tiny and spaced wide beneath.

**Signature detail:** The RPM arc has a "comet tail" — the leading edge of the fill has a bright white glow that leaves a slight trail, making RPM changes feel fluid and alive.

---

## Concept 2 — "SPLIT HORIZON"

**Vibe:** Brutalist motorsport. Screen split exactly in half — left is SPEED territory, right is RPM territory. No overlap, pure bipartite contrast.

**Layout:**
```
┌──────────────────┬──────────────────┐
│                  │                  │
│                  │                  │
│      182         │      4200        │
│     km/h         │       rpm        │
│                  │                  │
│                  │                  │
│──────────────────┼──────────────────│
│ D4  FUEL 72%     │ TRIP 34.2km      │
│     ━━━━━━░░     │ RNG  410km       │
└──────────────────┴──────────────────┘
```

**Speed (left half):** Full left panel, speed digits vertically centered, right-aligned toward the divider. Enormous bold condensed typeface. Background subtly darker.

**RPM (right half):** Full right panel, RPM digits vertically centered, left-aligned toward the divider. Slightly smaller than speed but still huge. Background tinted with a live color wash — green at idle, amber at 3500+, red bleed at 5500+.

**Divider:** A 2px vertical hairline in accent color, softly glowing.

**Fuel:** Bottom-left quadrant. Minimal text + thin track bar.

**Trip + Range:** Bottom-right quadrant. Stacked micro-text.

**Gear:** Bold uppercase badge tucked into the bottom-left corner.

**Colors:** Left half: matte black (#0a0a0a). Right half: very dark charcoal (#111111) with RPM-reactive color wash. Speed in pure white. RPM in the accent color. Accent: a single neon — either electric green (#39ff14) or hot orange (#ff6b35), chosen at build time.

**Typography:** Heavy condensed industrial sans (Impact / Bebas Neue family). All uppercase. Letters tightly packed.

**Signature detail:** The right-half background color shifts smoothly based on RPM — it breathes. At idle it's barely tinted, at redline the entire right panel washes in deep crimson.

---

## Concept 3 — "BLACKOUT DIAL"

**Vibe:** Luxury analog meets digital. A single large virtual analog speedometer dial with the RPM expressed as a ring around it — like a high-end Swiss watch face.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│         60  80  100 120  140        │
│       40 ╭──────────────╮ 160      │
│     20 ╱    ┏━━━━━━━┓    ╲ 180     │
│    0  │     ┃  124  ┃     │ 200    │
│       │     ┃ km/h  ┃     │        │
│     ──│     ┗━━━━━━━┛     │──      │
│       │  ▲                 │        │
│        ╲  needle           ╱        │
│          ╰────────────────╯         │
│        [D4] ● 72%  ───  410km      │
│                  TRIP 34.2 km       │
└─────────────────────────────────────┘
```

**Speed:** Dual representation — a physical-style needle sweeping around the dial PLUS a large centered digital readout inside the dial face. The dial face has etched markings at every 20 km/h with minor ticks at every 10.

**RPM:** A thin luminous ring (6px) running around the outer border of the speedometer dial. The fill arc represents current RPM as a proportion of max. Color shifts from white → amber → red.

**Fuel + Range:** A compact one-line readout beneath the dial. Dot separator.

**Trip:** Tiny odometer-style text, bottom center.

**Gear:** Small rounded rectangle badge, left of the fuel line.

**Colors:** Jet black background (#000000). Dial face in very dark gray (#0d0d0d) with a subtle circular brushed-metal texture (CSS radial gradient). Dial markings in dim silver (#666). Needle in bright red (#ff1a1a). Speed digits in pure white. RPM ring in cyan-to-red gradient.

**Typography:** Clean geometric sans (DIN / Barlow family) for the digital speed. Dial numerals in a lighter weight. Everything is restrained and precise.

**Signature detail:** The needle has inertia — it overshoots slightly and settles, giving a mechanical feel. The digital readout updates instantly while the needle lags, creating a satisfying tension between precision and analog warmth.

---

## Concept 4 — "TYPEFACE ONLY"

**Vibe:** Pure typography. No gauges, no arcs, no graphics. Just numbers and text dominating the screen with beautiful typographic hierarchy. Think: a billboard for your speed.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│                                     │
│                                     │
│           182                       │
│                                     │
│                                     │
│   km/h              4,200 rpm       │
│                                     │
│   72% fuel          410 km range    │
│   D4                34.2 km trip    │
└─────────────────────────────────────┘
```

**Speed:** One massive number — fills nearly 60% of the screen height. Left-aligned with generous left margin. Ultra-light weight (100–200). The sheer size creates hierarchy without any visual decoration.

**RPM:** Right-aligned, same baseline as the "km/h" label. Medium weight, smaller size (~40% of speed size). Comma-formatted for readability.

**Unit labels:** Extremely small, letterspaced wide, placed below-left of speed and inline with RPM.

**Fuel + Range:** Two lines of small body text, bottom-left. Equal weight, muted color.

**Trip:** Bottom-right, same size as fuel text.

**Gear:** Bottom-left, bold, same line as fuel.

**Colors:** Two options —
- *Light mode:* Pure white bg (#ffffff), speed in near-black (#111), RPM in medium gray (#888), metadata in light gray (#bbb).
- *Dark mode:* Pure black bg (#000000), speed in near-white (#f0f0f0), RPM in medium gray (#666), metadata in dark gray (#444).

**Typography:** A single elegant variable font at different weights. Preferably a humanist sans like Inter or a display serif like Playfair Display. The beauty comes entirely from weight contrast, size contrast, and whitespace.

**Signature detail:** Speed digits use tabular (monospace) figures so the layout never shifts. When speed changes, digits do a subtle vertical slide transition (each digit scrolls independently like a mechanical counter).

---

## Concept 5 — "NEON TACHOMETER"

**Vibe:** JDM street racer. Full-width horizontal RPM bar across the top, speed stacked below. Aggressive, unapologetic, neon-drenched.

**Layout:**
```
┌─────────────────────────────────────┐
│ 0    1    2    3    4    5    6  RPM│
│ ████████████████████░░░░░░░░░░░░░░ │
│                                     │
│              182                    │
│             km/h                    │
│                                     │
│  D4    72% ━━━━━━━░░  410km        │
│                        TRIP 34.2km  │
└─────────────────────────────────────┘
```

**RPM:** A massive full-width horizontal bar spanning the entire top third of the screen. Thick (~60px). LED-segment style with individual blocks that light up left to right. Below 4000: green blocks. 4000–5000: yellow. 5000+: red. Each block has a slight glow/bloom effect. Tick labels above in small monospace text.

**Speed:** Centered below the tach bar. Big, bold, condensed numerals. A subtle underline glow in the dominant RPM color (so if RPM is in green zone, speed has a faint green underglow).

**Unit:** Small label centered beneath speed.

**Fuel:** Horizontal micro-bar, bottom area. Simple percentage + range.

**Gear:** Aggressive square badge, bottom-left, filled background.

**Colors:** Pitch black bg (#000). RPM blocks: `#00ff41` (green) → `#ffe400` (yellow) → `#ff0040` (red). Speed in white. All secondary text in dim green (#00ff41 at 40% opacity). Scan-line overlay at 3% opacity for CRT effect.

**Typography:** Monospace or pixel-grid font (VT323 / IBM Plex Mono / custom bitmap style). Everything screams retro-digital.

**Signature detail:** The RPM blocks have a "peak hold" marker — a single bright block stays at the highest RPM reached for 1.5 seconds before dropping, like a real shift light array. At redline, the entire bar flashes.

---

## Concept 6 — "GLASS COCKPIT"

**Vibe:** Modern aircraft primary flight display (PFD). Clean grid layout, no curves — everything is rectilinear, layered glass panels with subtle depth.

**Layout:**
```
┌─────────────────────────────────────┐
│ ┌───────────────────────────────┐   │
│ │  SPEED              RPM      │   │
│ │ ┌──────────┐   ┌──────────┐  │   │
│ │ │          │   │          │  │   │
│ │ │   182    │   │  4,200   │  │   │
│ │ │   km/h   │   │   rpm    │  │   │
│ │ │          │   │          │  │   │
│ │ └──────────┘   └──────────┘  │   │
│ ├───────────────────────────────┤   │
│ │  D4  │ FUEL 72%  410km │TRIP │   │
│ │      │ ━━━━━━━━━━░░░░░ │34.2 │   │
│ └───────────────────────────────┘   │
└─────────────────────────────────────┘
```

**Speed:** Left glass card — frosted semi-transparent panel with subtle border. Speed digits centered within the card. Large but not extreme. Clean.

**RPM:** Right glass card — same dimensions as speed card. Mirror layout. RPM value slightly different color to distinguish at a glance.

**Cards:** Both cards have a 1px border in accent color at ~15% opacity, a frosted glass background (backdrop-filter: blur), and a very subtle top-edge highlight.

**Bottom bar:** A narrower glass panel spanning the full width, divided into three equal cells: Gear | Fuel+Range | Trip.

**Colors:** Deep navy gradient bg (#0a1628 → #0e1f3a). Cards in frosted glass (rgba white at 5% with blur). Speed in white. RPM in soft cyan (#7dd3fc). Labels in muted blue-gray. Bottom bar slightly brighter glass.

**Typography:** System UI / SF Pro style — clean, neutral, highly legible. Medium weight for values, light weight for labels.

**Signature detail:** Cards have a very subtle parallax depth effect — a barely-perceptible shadow shift when speed changes, implying the card is floating above the background layer. When OBD drops, the cards "sink" (shadow reduces, opacity dims slightly).

---

## Concept 7 — "RADIAL TWIN"

**Vibe:** Twin circular gauges side by side — classic performance car cluster digitally reborn. Speed gauge left, RPM gauge right, linked by a shared visual band.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│    ╭────────╮       ╭────────╮      │
│   ╱ 240° ARC╲     ╱ 240° ARC╲     │
│  │   ┌───┐   │   │   ┌───┐   │    │
│  │   │182│   │───│   │4.2│   │    │
│  │   │kph│   │   │   │×k │   │    │
│  │   └───┘   │   │   └───┘   │    │
│   ╲__________╱     ╲__________╱     │
│                                     │
│   D4    72% fuel  410km   34.2km    │
└─────────────────────────────────────┘
```

**Speed gauge (left):** ~200px diameter 240° sweep arc with tick marks every 20 km/h. Arc fill shows current speed as a bright sweep. Digital readout centered inside.

**RPM gauge (right):** Same size and geometry. Arc fill for RPM. Digital readout in thousands (e.g., "4.2" with "×k" subscript).

**Connection:** A thin horizontal line or subtle gradient band connects the two gauges at their equator, unifying them as one instrument cluster.

**Bottom strip:** Simple text row — Gear, fuel %, range, trip. All in one line, evenly spaced.

**Colors:** Charcoal bg (#151515). Speed arc: electric blue (#00b4d8) fill on dark track. RPM arc: amber (#f59e0b) fill on dark track. Both arcs have a soft outer glow. Center readouts in white. Bottom strip in muted gray.

**Typography:** Rounded geometric sans (Nunito / Quicksand family). Friendly but precise. Speed and RPM digits in bold, everything else in regular weight.

**Signature detail:** The two arcs "breathe" with a very slow pulse (opacity oscillates ±3%) to prevent the static display from feeling dead. When either value changes rapidly, the pulse quickens briefly.

---

## Concept 8 — "VERTICAL STACK"

**Vibe:** Smartphone-native. Full portrait-logic even on landscape — speed stacked on top of RPM like a tall infographic. Minimal, editorial, magazine-cover energy.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│              182                    │
│              ───                    │
│             km/h                    │
│                                     │
│         ▁▂▃▄▅▆▇█▇▆▅  4,200 rpm     │
│                                     │
│  ────────────────────────────────── │
│                                     │
│   D4     72%  ━━━━━━░░   410km     │
│                          34.2 trip  │
└─────────────────────────────────────┘
```

**Speed:** Top 55% of screen. Centered. Huge. A thin horizontal rule sits below it as a visual divider, with "km/h" label centered beneath the rule.

**RPM:** Represented as a small histogram / bar-graph waveform band — a horizontal strip of thin vertical bars whose height represents recent RPM history (last ~30 data points scrolling left). Current RPM numeric value sits to the right of the waveform. This creates a living, breathing heart-monitor feel.

**Divider:** A full-width 1px line separates the speed zone from the info zone.

**Fuel + Range + Trip + Gear:** Single-row layout at the bottom. Compact, utilitarian.

**Colors:** Off-black bg (#0c0c0c). Speed in white (#fafafa). RPM waveform bars in gradient from teal (#2dd4bf at idle) to rose (#fb7185 at high RPM). Divider in very dim white (10% opacity). Bottom text in gray (#9ca3af).

**Typography:** Modern grotesque (Neue Haas Grotesk / Helvetica Neue / Inter). Extremely clean. Speed in thin weight (200), unit label in caps with wide tracking.

**Signature detail:** The RPM histogram scrolls continuously leftward, painting a trail of recent engine activity. You can literally see the driving rhythm as a waveform — gentle cruise shows a flat band, spirited acceleration shows dramatic spikes.

---

## Concept 9 — "EMBER RING"

**Vibe:** Dark luxury with fire metaphor. A single ring gauge where the fill is a stylized ember/flame gradient. The interior is a dark void with speed floating in it. Everything radiates warmth.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│           ╭━━━━━━━━━━━╮             │
│        ╱▓▓▓ EMBER RING ▓▓╲          │
│      ╱▓▓                 ▓▓╲        │
│     │▓                     ▓│       │
│     │     182  km/h         │       │
│     │     ────              │       │
│     │▓    4,200 rpm        ▓│       │
│      ╲▓▓                 ▓▓╱        │
│        ╲▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╱          │
│           ╰─────────────╯           │
│   D4   72% fuel  410km  34.2 trip   │
└─────────────────────────────────────┘
```

**Ring:** A 300° arc, thick (~32px). The fill represents RPM. The gradient goes from deep ember (#8b0000) at the start through orange (#ff6600) to bright molten gold (#ffcc00) at the leading edge. The unfilled portion is a dark charcoal track. A subtle particle effect (CSS animation with tiny radial-gradient dots) drifts upward from the leading edge, simulating sparks.

**Speed:** Centered inside the ring. Large, warm white text. Below it, a thin decorative line.

**RPM (numeric):** Smaller, centered below the speed, inside the ring. Muted warm gray.

**Bottom bar:** Single text line outside the ring. Simple and understated.

**Colors:** Deep black bg with a warm tint (#0a0604). Ring ember gradient. Speed text in warm white (#fff5e6). RPM text in warm gray (#b8a898). Secondary text in dim brown-gray (#6b5d50). When fuel is low, the ring cools to blue, signaling "running out of fire."

**Typography:** Elegant serif or slab-serif for the speed value (Playfair Display / Roboto Slab). Sans-serif for labels. The contrast between serif speed and sans labels creates visual interest.

**Signature detail:** The ember ring leading edge has a subtle animated glow that flickers slightly, like a real flame. At idle, it's a gentle warm pulse. At high RPM, the glow intensifies and the "sparks" increase. The overall effect is a dashboard that feels alive and warm.

---

## Concept 10 — "HEADS-UP WIRE"

**Vibe:** Minimalist HUD projection aesthetic. Looks like it's being projected onto the windshield — no solid backgrounds, just wireframe elements and floating data points on near-invisible structure.

**Layout:**
```
┌─────────────────────────────────────┐
│                                     │
│     ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐       │
│     ╎                       ╎       │
│     ╎        182            ╎       │
│     ╎        km/h           ╎       │
│     ╎                       ╎       │
│     └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘       │
│                                     │
│  ╌╌╌╌╌╌╌╌╌╌╌╌╌│╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌  │
│   0          4200              6000  │
│                                     │
│  D4    72%        410km    34.2km   │
└─────────────────────────────────────┘
```

**Speed:** Floating inside a dashed rectangular boundary — the border is rendered as a subtle dashed line (1px, ~20% opacity), implying a containment zone without filling it. Speed digits in a single bright color. The dashed box gives a targeting-reticle / cockpit-HUD feel.

**RPM:** A horizontal wireframe bar below the speed zone. Just a thin outlined track (not filled — only the outline is drawn) with a bright vertical needle/marker at the current RPM position. Small tick labels at 0, 2000, 4000, 6000. The marker has a trailing ghost (previous position shown at lower opacity for 0.5s).

**Fuel + Range + Trip + Gear:** Spaced along the bottom as floating text nodes — no backgrounds, no cards, just text hanging in space connected by thin dashed alignment lines.

**Colors:** True black bg (#000000). All wireframes and text in a single HUD color — either phosphor green (#33ff33) or HUD cyan (#00ffcc) or amber (#ffaa00). User-selectable. Everything is one color at varying opacities to mimic monochrome HUD projection.

**Typography:** Monospace (JetBrains Mono / Fira Code / Courier). Fixed-width characters reinforce the HUD/terminal aesthetic. Speed is large, RPM and metadata are small but proportionally scaled.

**Signature detail:** All elements have a very subtle scan-line effect (1px horizontal lines at ~5% opacity) and a barely-visible CRT flicker animation (opacity oscillates between 97–100% over 2 seconds). The speed dashed border "sweeps" — one segment of the dashed border is slightly brighter and rotates around the perimeter slowly, like a radar sweep. The result feels like a real heads-up display projected onto glass.

---

## Comparison Matrix

| # | Concept | Speed Presentation | RPM Presentation | Layout | Personality |
|---|---------|-------------------|-----------------|--------|-------------|
| 1 | Orbital Arc | Center, inside ring | 270° thick arc ring | Concentric | Sci-fi futuristic |
| 2 | Split Horizon | Left half, massive | Right half, massive | 50/50 bipartite | Brutalist motorsport |
| 3 | Blackout Dial | Analog needle + digital | Outer ring of dial | Single dial | Luxury analog |
| 4 | Typeface Only | Giant text, 60% height | Body text, right-aligned | Pure typography | Editorial minimal |
| 5 | Neon Tachometer | Below tach bar | Full-width LED bar | Horizontal stacked | JDM street racer |
| 6 | Glass Cockpit | Left glass card | Right glass card | Side-by-side cards | Aviation clinical |
| 7 | Radial Twin | Left circular gauge | Right circular gauge | Twin dials | Classic sport |
| 8 | Vertical Stack | Top 55% of screen | Scrolling histogram | Vertical editorial | Magazine infographic |
| 9 | Ember Ring | Inside ring, serif | Ring fill (ember) | Centered ring | Dark luxury warmth |
| 10 | Heads-Up Wire | Dashed bounding box | Wireframe bar + needle | Floating wireframe | HUD monochrome |

---

## Implementation Notes

- Each concept should be implementable as a single HTML + CSS + JS file with no external dependencies beyond a web font.
- All concepts render into a 480×320 viewport (CSS `aspect-ratio: 3/2` container).
- Speed and RPM values are fed from the existing WebSocket telemetry schema (`spd`, `rpm`, `fuel`, `rng` fields).
- Concept selection can be driven by a `data-design` attribute on `<body>` or a URL query parameter for preview.
- Animations should respect `prefers-reduced-motion`.

## Next Step

Review and select 1–3 concepts for prototype implementation. Each prototype will be a standalone preview page before integration into the main dashboard pipeline.
