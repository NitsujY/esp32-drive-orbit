# Dashboard Source And Preview Workflow

The active dashboard flow is HTTP-first and matches the direct ESP32 use case.

- Legacy sandbox `preview/dashboard_preview/` has been removed. The authored dashboard now lives in `frontend/dashboard/`, and `preview/` is only for supporting host-side prototypes.

- `frontend/dashboard/`: source HTML, CSS, and JavaScript for the production dashboard UI
- `tools/dashboard/dev-server.mjs`: local static server with mock telemetry or optional WebSocket proxy mode
- `tools/dashboard/build-target.mjs`: shared build pipeline for all dashboard deployment targets
- `tools/dashboard/build.mjs`: builds and gzips the frontend into `data/` for LittleFS upload
- `tools/dashboard/build-native.mjs`: builds the same frontend into `build/capacitor/` for native shells
- `tools/dashboard/release-server.mjs`: serves the packaged `data/` assets for sign-off on the built bundle
- `preview/companion/`: host-side orb companion prototype
- `preview/features/`: host-side feature concept board
- `data/`: generated deployment assets served by the ESP32 through AsyncWebServer
- `build/capacitor/`: generated web bundle consumed by Capacitor native shells
- `ios/`: standard Capacitor iOS project root

## Local Mac Development

Install the Node tooling once from the repository root:

```sh
npm install
```

Start the local source preview with mock telemetry:

```sh
npm run dev
```

This mode is fast for iteration, but it is not the exact ESP32 packaging path:

- it serves files directly from `frontend/dashboard/`
- it uses a mock WebSocket feed unless you opt into proxy mode

## Single Source Of Truth

`frontend/dashboard/` is the only authored source for the dashboard HTML, CSS, and JS.

- `data/` is generated output, not a second hand-maintained frontend
- the ESP32 serves the generated `data/` bundle

For exact UI bundle parity with what the ESP32 serves, use:

```sh
npm run build:web
node tools/dashboard/release-server.mjs
```

## Native iPhone Shell

The repo now supports a Capacitor iPhone shell without changing the frontend source tree.

Folder architecture:

- `frontend/dashboard/`: shared dashboard source for browser, ESP32, and native shells
- `data/`: ESP32 deployment output
- `build/capacitor/`: generated native-shell web bundle
- `ios/`: native iOS project

Build, sync, install, and launch the iPhone app shell on a connected device:

```sh
npm run cap:run:ios
```

If you need to inspect signing or native settings manually, open the generated iOS project in Xcode directly:

```sh
npx cap open ios
```

The Capacitor shell bundles the UI locally and connects telemetry to the ESP32 over the LAN. Its default endpoint is:

- `ws://carconsole.local/ws`

If mDNS fails on a given network, replace it in the in-app settings with the board IP, for example:

- `ws://192.168.1.52/ws`

The iOS project is configured for:

- local-network permission prompts
- Bonjour `_http._tcp` discovery support
- local cleartext web traffic from the embedded WKWebView to the ESP32

Recommended install path for your own iPhone:

1. Run `npm run cap:run:ios` once signing is already configured.
2. If Xcode prompts for signing fixes, run `npx cap open ios`.
3. In Xcode, choose your Apple team under Signing.
4. Connect your iPhone and press Run.

For easier re-installs and updates later, ship the same Capacitor app through TestFlight.

## Switching Between Mock Preview And Live ESP32 Data

The dashboard reads its WebSocket endpoint from:

1. `?ws=...` query parameter if present
2. saved `localStorage` override
3. same-origin `/ws` by default

Examples:

- local mock preview: `http://127.0.0.1:8123/`
- direct live preview to ESP32: `http://127.0.0.1:8123/?ws=ws://192.168.1.52/ws`

The preview servers also support a same-origin proxy mode, which is useful when you want the preview page to stay on one origin while forwarding `/ws` to the ESP32:

```sh
npm run dev -- --proxy-ws=ws://192.168.1.52/ws
```

or for the built bundle:

```sh
npm run build:web
node tools/dashboard/release-server.mjs --proxy-ws=ws://192.168.1.52/ws
```

## Direct ESP32 Runtime

The real product path is the direct board page over HTTP:

- `http://<esp32-ip>/`
- `http://carconsole.local/` on networks where mDNS works

This repo no longer targets HTTPS/PWA-only browser features in the direct ESP32 runtime.

## External Device Verification

For devices on the same LAN, use the LAN URL printed by `npm run dev` or `node tools/dashboard/release-server.mjs`.

For real board testing on a phone, open the ESP32 directly:

- `http://<esp32-ip>/`

Notes:

- many phone hotspots isolate clients or break multicast discovery, so `carconsole.local` may fail even when direct IP works
- use `--proxy-ws=ws://<esp32-ip>/ws` if you want the local preview server to bridge live telemetry from the board

## Build And Upload To ESP32

Generate minified and gzipped frontend assets:

```sh
npm run build:web
```

Build the firmware:

```sh
pio run -e dash_35
```

Upload the LittleFS image:

```sh
pio run -e dash_35 -t uploadfs
```

Then upload firmware normally:

```sh
pio run -e dash_35 -t upload
```

After the ESP32 joins the hotspot, the dashboard should be reachable by direct IP and, on networks that support multicast mDNS, at `http://carconsole.local/`.
