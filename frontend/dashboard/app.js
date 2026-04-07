const elements = {
  socketStatus: document.getElementById('socket-status'),
  freshnessStatus: document.getElementById('freshness-status'),
  topStrip: document.querySelector('.top-strip'),
  gaugeMeta: document.querySelector('.gauge-meta'),
  settingsToggle: document.getElementById('settings-toggle'),
  settingsPanel: document.getElementById('settings-panel'),
  settingsBackdrop: document.getElementById('settings-backdrop'),
  settingsClose: document.getElementById('settings-close'),
  wsUrl: document.getElementById('ws-url'),
  connectButton: document.getElementById('connect-button'),
  resetButton: document.getElementById('reset-button'),
  simulationButton: document.getElementById('simulation-button'),
  simulationStatus: document.getElementById('simulation-status'),
  cacheResetButton: document.getElementById('cache-reset-button'),
  cacheStatus: document.getElementById('cache-status'),
  heatDebugToggle: document.getElementById('heat-debug-toggle'),
  heatDebugHint: document.getElementById('heat-debug-hint'),
  heatDebugOverlay: document.getElementById('heat-debug-overlay'),
  heatDebugClose: document.getElementById('heat-debug-close'),
  heatDebugSummary: document.getElementById('heat-debug-summary'),
  heatRenderToggle: document.getElementById('heat-render-toggle'),
  heatGpsToggle: document.getElementById('heat-gps-toggle'),
  heatDebugOutput: document.getElementById('heat-debug-output'),
  smartNote: document.getElementById('smart-note'),
  speedValue: document.getElementById('speed-value'),
  speedPhase: document.getElementById('speed-phase'),
  rpmValue: document.getElementById('rpm-value'),
  accelValue: document.getElementById('accel-value'),
  accelMarker: document.getElementById('accel-marker'),
  lightsValue: document.getElementById('lights-value'),
  gpsValue: document.getElementById('gps-value'),
  weatherValue: document.getElementById('weather-value'),
  fuelValue: document.getElementById('fuel-value'),
  fuelFill: document.getElementById('fuel-fill'),
  rangeValue: document.getElementById('range-value'),
  focusFuelValue: document.getElementById('focus-fuel-value'),
  focusRangeValue: document.getElementById('focus-range-value'),
  wifiStatus: document.getElementById('wifi-status'),
  obdStatus: document.getElementById('obd-status'),
  gaugeCanvas: document.getElementById('gauge-canvas'),
  themeColor: document.querySelector('meta[name="theme-color"]'),
};

const gaugeContext = elements.gaugeCanvas.getContext('2d');
const maxGaugeRpm = 6000;
const rpmPrimaryBandCeiling = 2000;
const rpmPrimaryBandRatio = 0.75;
const focusModeEnterSpeedKph = 28;
const focusModeExitSpeedKph = 20;
const focusModeEnterRpm = 1650;
const focusModeExitRpm = 1280;
const focusModeCommitDelayMs = 920;
const slowCruiseEnterEmphasis = 0.46;
const slowCruiseExitEmphasis = 0.22;

const targetState = makeSnapshot();
const displayedState = makeSnapshot();
const slowState = makeSnapshot();
const lastFreshState = makeSnapshot();

const slowTelemetryIntervalMs = 900;
const telemetryHoldMs = 2800;
const baseGpsSampleIntervalMs = 45000;
const focusGpsSampleIntervalMs = 60000;
const gpsRequestTimeoutMs = 12000;
const simulationCycleSeconds = 58;
const smartNoteIntervalMs = 2600;
const activeAnimationFrameIntervalMs = 50;
const retainedTelemetryKeys = ['clt', 'wt', 'wc', 'bat', 'fuel', 'rng', 'hl'];

let socket = null;
let reconnectTimer = null;
let reconnectAttempt = 0;
let lastFreshTelemetryAt = 0;
let lastSlowCommitAt = 0;
let currentSocketUrl = resolveInitialSocketUrl();
let settingsOpen = false;
let noteIndex = 0;
let lastNoteAt = 0;
let simulationMode = resolveInitialSimulationMode();
let simulationSequence = 0;
let simulationPhase = 0;
let simulationUptimeMs = 0;
let lastSimulationTickAt = 0;
let gpsPollTimer = null;
let gpsPollingStarted = false;
let gpsRequestInFlight = false;
let focusModeActive = false;
let slowCruiseModeActive = false;
let animationFrameId = null;
let renderWakeTimer = null;

const heatDebugState = {
  enabled: resolveInitialHeatDebugEnabled(),
  renderPaused: false,
  gpsPaused: false,
  uiTimer: null,
  windowStartedAt: performance.now(),
  frameWindowCount: 0,
  frameWindowCostMs: 0,
  domUpdatesWindow: 0,
  gaugeDrawsWindow: 0,
  socketMessagesWindow: 0,
  fps: 0,
  avgFrameCostMs: 0,
  domUpdatesPerSecond: 0,
  gaugeDrawsPerSecond: 0,
  socketMessagesPerSecond: 0,
  domUpdates: 0,
  gaugeDraws: 0,
  socketMessages: 0,
  gpsRequests: 0,
  gpsSuccesses: 0,
  gpsErrors: 0,
  gpsLastDurationMs: 0,
  gpsLastCompletedAt: 0,
};

const motionState = {
  lastSampleAt: 0,
  lastSpeed: 0,
  lastAccel: 0,
  variationTarget: 0.7,
  variationDisplay: 0.7,
};

const focusModeState = {
  pendingTarget: null,
  pendingSince: 0,
};

const gpsState = {
  supported: typeof navigator.geolocation !== 'undefined',
  status: typeof navigator.geolocation !== 'undefined' ? 'idle' : 'unsupported',
  latitude: null,
  longitude: null,
  accuracyM: null,
  updatedAt: 0,
  error: '',
};

const renderEpsilon = {
  spd: 0.05,
  rpm: 8,
  clt: 0.2,
  bat: 8,
  fuel: 0.08,
  rng: 0.25,
  acc: 8,
  variation: 0.03,
};

function isNativeShell() {
  return typeof window.Capacitor?.isNativePlatform === 'function' && window.Capacitor.isNativePlatform();
}

function resolveDefaultSocketUrl() {
  if (isNativeShell()) {
    return 'ws://carconsole.local/ws';
  }

  return `ws://${window.location.host}/ws`;
}

function resizeGaugeCanvas() {
  const { gaugeCanvas } = elements;
  const bounds = gaugeCanvas.getBoundingClientRect();
  const nextWidth = Math.max(320, Math.round(bounds.width));
  const nextHeight = Math.max(180, Math.round(bounds.height));

  if (gaugeCanvas.width === nextWidth && gaugeCanvas.height === nextHeight) {
    return;
  }

  gaugeCanvas.width = nextWidth;
  gaugeCanvas.height = nextHeight;
}

function makeSnapshot() {
  return {
    v: 1,
    seq: 0,
    up: 0,
    spd: 0,
    rpm: 0,
    clt: 0,
    wt: -128,
    wc: 255,
    bat: 0,
    fuel: 0,
    rng: 0,
    acc: 0,
    wifi: 0,
    hl: 0,
    obd: 0,
    fresh: 0,
  };
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function lerp(current, target, factor) {
  return current + (target - current) * factor;
}

function settleValue(current, target, factor, epsilon) {
  const next = lerp(current, target, factor);
  return isSettled(next, target, epsilon) ? target : next;
}

function resolveInitialSimulationMode() {
  const params = new URLSearchParams(window.location.search);
  const queryValue = params.get('simulation');

  if (queryValue === '1' || queryValue === 'true' || queryValue === 'on') {
    localStorage.setItem('driveOrbitSimulation', '1');
    return true;
  }

  if (queryValue === '0' || queryValue === 'false' || queryValue === 'off') {
    localStorage.removeItem('driveOrbitSimulation');
    return false;
  }

  return localStorage.getItem('driveOrbitSimulation') === '1';
}

function parseToggleQuery(value) {
  if (typeof value !== 'string') {
    return null;
  }

  const normalized = value.trim().toLowerCase();
  if (['1', 'true', 'on', 'yes', 'heat'].includes(normalized)) {
    return true;
  }

  if (['0', 'false', 'off', 'no'].includes(normalized)) {
    return false;
  }

  return null;
}

function resolveInitialHeatDebugEnabled() {
  const params = new URLSearchParams(window.location.search);
  const heatFlag = parseToggleQuery(params.get('heat'));

  if (heatFlag !== null) {
    if (heatFlag) {
      localStorage.setItem('driveOrbitHeatDebug', '1');
    } else {
      localStorage.removeItem('driveOrbitHeatDebug');
    }

    return heatFlag;
  }

  if (params.get('debug')?.trim().toLowerCase() === 'heat') {
    localStorage.setItem('driveOrbitHeatDebug', '1');
    return true;
  }

  return localStorage.getItem('driveOrbitHeatDebug') === '1';
}

function resolveInitialSocketUrl() {
  const params = new URLSearchParams(window.location.search);
  const queryValue = params.get('ws');
  if (queryValue) {
    localStorage.setItem('driveOrbitWsUrl', queryValue);
    return queryValue;
  }

  const savedValue = localStorage.getItem('driveOrbitWsUrl');
  if (savedValue) {
    return savedValue;
  }

  return resolveDefaultSocketUrl();
}

function updateRuntimeUi() {
  const defaultSocketUrl = resolveDefaultSocketUrl();
  elements.resetButton.textContent = isNativeShell() ? 'Use App Default' : 'Use Same-Origin';
  elements.wsUrl.placeholder = defaultSocketUrl;
}

function scheduleAnimationLoop() {
  if (animationFrameId !== null || heatDebugState.renderPaused || document.visibilityState !== 'visible') {
    return;
  }

  clearRenderWakeTimer();
  animationFrameId = window.requestAnimationFrame(runAnimationFrame);
}

function cancelAnimationLoop() {
  if (animationFrameId === null) {
    clearRenderWakeTimer();
    return;
  }

  window.cancelAnimationFrame(animationFrameId);
  animationFrameId = null;
  clearRenderWakeTimer();
}

function runAnimationFrame(now = performance.now()) {
  animationFrameId = null;
  animate(now);
}

function clearRenderWakeTimer() {
  if (renderWakeTimer === null) {
    return;
  }

  window.clearTimeout(renderWakeTimer);
  renderWakeTimer = null;
}

function scheduleRenderWake(delayMs = 0) {
  if (heatDebugState.renderPaused || document.visibilityState !== 'visible') {
    clearRenderWakeTimer();
    return;
  }

  if (delayMs <= 16) {
    scheduleAnimationLoop();
    return;
  }

  clearRenderWakeTimer();
  renderWakeTimer = window.setTimeout(() => {
    renderWakeTimer = null;
    scheduleAnimationLoop();
  }, delayMs);
}

function isSettled(current, target, epsilon) {
  return Math.abs(current - target) <= epsilon;
}

function hasUnsettledDisplayState() {
  return !isSettled(displayedState.spd, targetState.spd, renderEpsilon.spd) ||
    !isSettled(displayedState.rpm, targetState.rpm, renderEpsilon.rpm) ||
    !isSettled(displayedState.clt, slowState.clt, renderEpsilon.clt) ||
    !isSettled(displayedState.bat, slowState.bat, renderEpsilon.bat) ||
    !isSettled(displayedState.fuel, slowState.fuel, renderEpsilon.fuel) ||
    !isSettled(displayedState.rng, slowState.rng, renderEpsilon.rng) ||
    !isSettled(displayedState.acc, targetState.acc, renderEpsilon.acc) ||
    !isSettled(motionState.variationDisplay, motionState.variationTarget, renderEpsilon.variation);
}

function resolveFreshnessState(now = performance.now()) {
  if (lastFreshTelemetryAt === 0) {
    return {
      text: 'Stale',
      variant: 'error',
      nextWakeDelayMs: null,
    };
  }

  const ageMs = now - lastFreshTelemetryAt;
  if (ageMs <= slowTelemetryIntervalMs) {
    return {
      text: 'Fresh',
      variant: 'live',
      nextWakeDelayMs: Math.max(0, slowTelemetryIntervalMs - ageMs),
    };
  }

  if (ageMs <= telemetryHoldMs) {
    return {
      text: 'Hold',
      variant: 'pending',
      nextWakeDelayMs: Math.max(0, telemetryHoldMs - ageMs),
    };
  }

  return {
    text: 'Stale',
    variant: 'error',
    nextWakeDelayMs: null,
  };
}

function resolveNextSimulationWakeDelay(now = performance.now()) {
  if (!simulationMode) {
    return null;
  }

  if (lastSimulationTickAt === 0) {
    return 0;
  }

  return Math.max(0, 125 - (now - lastSimulationTickAt));
}

function resolveNextRenderDelay(now, nextNoteDelayMs, nextFreshnessDelayMs) {
  const candidateDelays = [];

  if (hasUnsettledDisplayState()) {
    candidateDelays.push(activeAnimationFrameIntervalMs);
  }

  const nextSimulationDelayMs = resolveNextSimulationWakeDelay(now);
  if (nextSimulationDelayMs != null) {
    candidateDelays.push(nextSimulationDelayMs);
  }

  if (nextNoteDelayMs != null) {
    candidateDelays.push(nextNoteDelayMs);
  }

  if (nextFreshnessDelayMs != null) {
    candidateDelays.push(nextFreshnessDelayMs);
  }

  if (candidateDelays.length === 0) {
    return null;
  }

  return Math.max(0, Math.min(...candidateDelays));
}

function resetHeatDebugWindow() {
  heatDebugState.windowStartedAt = performance.now();
  heatDebugState.frameWindowCount = 0;
  heatDebugState.frameWindowCostMs = 0;
  heatDebugState.domUpdatesWindow = 0;
  heatDebugState.gaugeDrawsWindow = 0;
  heatDebugState.socketMessagesWindow = 0;
}

function flushHeatDebugWindow(now = performance.now()) {
  const elapsedMs = Math.max(1, now - heatDebugState.windowStartedAt);

  heatDebugState.fps = heatDebugState.frameWindowCount === 0
    ? 0
    : (heatDebugState.frameWindowCount * 1000) / elapsedMs;
  heatDebugState.avgFrameCostMs = heatDebugState.frameWindowCount === 0
    ? 0
    : heatDebugState.frameWindowCostMs / heatDebugState.frameWindowCount;
  heatDebugState.domUpdatesPerSecond = (heatDebugState.domUpdatesWindow * 1000) / elapsedMs;
  heatDebugState.gaugeDrawsPerSecond = (heatDebugState.gaugeDrawsWindow * 1000) / elapsedMs;
  heatDebugState.socketMessagesPerSecond = (heatDebugState.socketMessagesWindow * 1000) / elapsedMs;

  resetHeatDebugWindow();
}

function formatDuration(ms) {
  if (!Number.isFinite(ms) || ms < 0) {
    return '--';
  }

  if (ms < 1000) {
    return `${Math.round(ms)}ms`;
  }

  if (ms < 60000) {
    return `${(ms / 1000).toFixed(1)}s`;
  }

  return `${Math.round(ms / 1000)}s`;
}

function resolveHeatDebugSummary() {
  if (heatDebugState.renderPaused) {
    return 'Rendering paused. If the phone cools now, the canvas loop was the dominant load.';
  }

  if (heatDebugState.gpsPaused) {
    return 'GPS paused. If the phone cools now, location polling was contributing.';
  }

  if (heatDebugState.fps >= 40) {
    return 'Rendering is the only continuous workload here; GPS stays low-frequency.';
  }

  if (heatDebugState.socketMessagesPerSecond > 0.5) {
    return 'The app is mostly reacting to live telemetry right now.';
  }

  return 'The app is mostly idle between telemetry and GPS events right now.';
}

function refreshHeatDebugUi() {
  elements.heatDebugToggle.textContent = heatDebugState.enabled ? 'Heat Diagnostics On' : 'Heat Diagnostics Off';
  elements.heatDebugToggle.setAttribute('aria-pressed', String(heatDebugState.enabled));
  elements.heatDebugHint.textContent = heatDebugState.enabled
    ? 'Overlay is live. Pause rendering or GPS for 60 seconds and feel the device.'
    : 'Enable live stats and isolate rendering versus GPS heat on-device.';
  elements.heatDebugOverlay.hidden = !heatDebugState.enabled;

  if (!heatDebugState.enabled) {
    return;
  }

  elements.heatRenderToggle.textContent = heatDebugState.renderPaused ? 'Resume Rendering' : 'Pause Rendering';
  elements.heatGpsToggle.textContent = heatDebugState.gpsPaused ? 'Resume GPS' : 'Pause GPS';
  elements.heatRenderToggle.setAttribute('aria-pressed', String(heatDebugState.renderPaused));
  elements.heatGpsToggle.setAttribute('aria-pressed', String(heatDebugState.gpsPaused));
  elements.heatDebugSummary.textContent = resolveHeatDebugSummary();

  const gpsAgeMs = gpsState.updatedAt === 0 ? Number.NaN : Date.now() - gpsState.updatedAt;

  elements.heatDebugOutput.textContent = [
    `fps ${heatDebugState.renderPaused ? '0 paused' : heatDebugState.fps.toFixed(0)} · frame ${heatDebugState.avgFrameCostMs.toFixed(1)}ms avg`,
    `gauge ${heatDebugState.gaugeDrawsPerSecond.toFixed(1)}/s · dom ${heatDebugState.domUpdatesPerSecond.toFixed(1)}/s`,
    `ws ${heatDebugState.socketMessagesPerSecond.toFixed(1)}/s · total ${heatDebugState.socketMessages}`,
    `gps req ${heatDebugState.gpsRequests} · ok ${heatDebugState.gpsSuccesses} · err ${heatDebugState.gpsErrors}`,
    `gps last ${formatDuration(heatDebugState.gpsLastDurationMs)} · age ${formatDuration(gpsAgeMs)} · status ${heatDebugState.gpsPaused ? 'paused' : gpsState.status}`,
  ].join('\n');
}

function startHeatDebugSampling() {
  if (heatDebugState.uiTimer !== null) {
    return;
  }

  resetHeatDebugWindow();
  heatDebugState.uiTimer = window.setInterval(() => {
    flushHeatDebugWindow();
    refreshHeatDebugUi();
  }, 1000);
}

function stopHeatDebugSampling() {
  if (heatDebugState.uiTimer !== null) {
    window.clearInterval(heatDebugState.uiTimer);
    heatDebugState.uiTimer = null;
  }

  heatDebugState.fps = 0;
  heatDebugState.avgFrameCostMs = 0;
  heatDebugState.domUpdatesPerSecond = 0;
  heatDebugState.gaugeDrawsPerSecond = 0;
  heatDebugState.socketMessagesPerSecond = 0;
  resetHeatDebugWindow();
}

function setHeatDebugEnabled(enabled) {
  heatDebugState.enabled = enabled;

  if (enabled) {
    localStorage.setItem('driveOrbitHeatDebug', '1');
    startHeatDebugSampling();
  } else {
    localStorage.removeItem('driveOrbitHeatDebug');

    if (heatDebugState.renderPaused) {
      heatDebugState.renderPaused = false;
      scheduleAnimationLoop();
    }

    if (heatDebugState.gpsPaused) {
      heatDebugState.gpsPaused = false;
      if (document.visibilityState === 'visible') {
        if (gpsPollingStarted) {
          refreshGpsPollingCadence();
        } else {
          startGpsPolling();
        }
      }
    }

    stopHeatDebugSampling();
  }

  refreshHeatDebugUi();
}

function setRenderPaused(paused) {
  heatDebugState.renderPaused = paused;

  if (paused) {
    cancelAnimationLoop();
    heatDebugState.fps = 0;
    heatDebugState.avgFrameCostMs = 0;
    heatDebugState.domUpdatesPerSecond = 0;
    heatDebugState.gaugeDrawsPerSecond = 0;
    resetHeatDebugWindow();
  } else {
    lastSimulationTickAt = 0;
    scheduleAnimationLoop();
  }

  refreshHeatDebugUi();
}

function setGpsPaused(paused) {
  heatDebugState.gpsPaused = paused;

  if (paused) {
    window.clearTimeout(gpsPollTimer);
    gpsPollTimer = null;
  } else if (document.visibilityState === 'visible') {
    requestGpsSample(true);

    if (gpsPollingStarted) {
      refreshGpsPollingCadence();
    } else {
      startGpsPolling();
    }
  }

  refreshHeatDebugUi();
  scheduleAnimationLoop();
}

function recordFrameMetrics(frameCostMs) {
  heatDebugState.frameWindowCount += 1;
  heatDebugState.frameWindowCostMs += frameCostMs;
}

function recordDomUpdate() {
  heatDebugState.domUpdates += 1;
  heatDebugState.domUpdatesWindow += 1;
}

function recordGaugeDraw() {
  heatDebugState.gaugeDraws += 1;
  heatDebugState.gaugeDrawsWindow += 1;
}

function recordSocketMessage() {
  heatDebugState.socketMessages += 1;
  heatDebugState.socketMessagesWindow += 1;
}

function resolveGpsSampleIntervalMs() {
  return focusModeActive ? focusGpsSampleIntervalMs : baseGpsSampleIntervalMs;
}

function resolveGpsMaximumAgeMs() {
  return resolveGpsSampleIntervalMs();
}

function resolveWeatherLabel(code, tempC) {
  const codeLabel = (() => {
    if (code === 0) return 'SUN';
    if (code === 1 || code === 2) return 'PART';
    if (code === 3) return 'CLOUD';
    if (code === 45 || code === 48) return 'FOG';
    if ([51, 53, 55, 56, 57].includes(code)) return 'DRIZZ';
    if ([61, 63, 65, 66, 67, 80, 81, 82].includes(code)) return 'RAIN';
    if ([71, 73, 75, 77, 85, 86].includes(code)) return 'SNOW';
    if ([95, 96, 99].includes(code)) return 'STORM';
    return 'WX';
  })();

  if (typeof tempC === 'number' && tempC > -100) {
    return `${codeLabel} ${Math.round(tempC)}C`;
  }

  return `${codeLabel} --`;
}

function resolveCoolantLabel(tempC) {
  if (typeof tempC !== 'number' || tempC <= -100) {
    return '--';
  }

  return `${Math.round(tempC)}C`;
}

function resolveBatteryLabel(batteryMv) {
  if (typeof batteryMv !== 'number' || batteryMv <= 0) {
    return '--';
  }

  return `${(batteryMv / 1000).toFixed(1)}V`;
}

function resolveAccelLabel(accelMg) {
  const accelG = (typeof accelMg === 'number' ? accelMg : 0) / 1000;
  const sign = accelG > 0 ? '+' : accelG < 0 ? '-' : '';
  return `${sign}${Math.abs(accelG).toFixed(2)}g`;
}

function resolveGpsInlineLabel() {
  if (heatDebugState.gpsPaused) {
    return 'Paused';
  }

  if (!gpsState.supported) {
    return 'No GPS';
  }

  if (gpsState.status === 'restricted') {
    return 'HTTPS req';
  }

  if (gpsState.status === 'denied') {
    return 'Denied';
  }

  if (gpsState.status === 'error') {
    return 'Error';
  }

  if (gpsState.status !== 'ready' || gpsState.accuracyM == null) {
    return 'Locating';
  }

  return `±${Math.round(gpsState.accuracyM)}m`;
}

function buildGpsNote() {
  if (gpsState.status !== 'ready' || gpsState.latitude == null || gpsState.longitude == null) {
    return null;
  }

  const lat = gpsState.latitude.toFixed(4);
  const lon = gpsState.longitude.toFixed(4);
  const accuracySuffix = gpsState.accuracyM == null ? '' : ` ±${Math.round(gpsState.accuracyM)}m`;
  return `GPS ${lat}, ${lon}${accuracySuffix}`;
}

function handleGpsPosition(position) {
  gpsState.status = 'ready';
  gpsState.latitude = position.coords.latitude;
  gpsState.longitude = position.coords.longitude;
  gpsState.accuracyM = position.coords.accuracy;
  gpsState.updatedAt = position.timestamp ?? Date.now();
  gpsState.error = '';
  refreshHeatDebugUi();
  scheduleAnimationLoop();
}

function handleGpsError(error) {
  if (error?.code === error?.PERMISSION_DENIED) {
    gpsState.status = 'denied';
  } else {
    gpsState.status = 'error';
  }

  gpsState.error = error?.message ?? 'gps_failed';
  refreshHeatDebugUi();
  scheduleAnimationLoop();
}

function requestGpsSample(forceLocating = false) {
  if (!gpsState.supported || gpsRequestInFlight || heatDebugState.gpsPaused) {
    return;
  }

  if (!isNativeShell() && !window.isSecureContext) {
    gpsState.status = 'restricted';
    return;
  }

  const requestStartedAt = performance.now();
  gpsRequestInFlight = true;
  heatDebugState.gpsRequests += 1;
  if (forceLocating || gpsState.status !== 'ready') {
    gpsState.status = 'locating';
    scheduleAnimationLoop();
  }

  navigator.geolocation.getCurrentPosition((position) => {
    gpsRequestInFlight = false;
    heatDebugState.gpsSuccesses += 1;
    heatDebugState.gpsLastDurationMs = performance.now() - requestStartedAt;
    heatDebugState.gpsLastCompletedAt = Date.now();
    handleGpsPosition(position);
  }, (error) => {
    gpsRequestInFlight = false;
    heatDebugState.gpsErrors += 1;
    heatDebugState.gpsLastDurationMs = performance.now() - requestStartedAt;
    heatDebugState.gpsLastCompletedAt = Date.now();
    handleGpsError(error);
  }, {
    enableHighAccuracy: false,
    timeout: gpsRequestTimeoutMs,
    maximumAge: resolveGpsMaximumAgeMs(),
  });
}

function scheduleGpsPoll(delayMs = resolveGpsSampleIntervalMs()) {
  if (!gpsState.supported || heatDebugState.gpsPaused) {
    window.clearTimeout(gpsPollTimer);
    gpsPollTimer = null;
    return;
  }

  window.clearTimeout(gpsPollTimer);
  gpsPollTimer = window.setTimeout(() => {
    gpsPollTimer = null;

    if (document.visibilityState === 'visible') {
      requestGpsSample(false);
    }

    scheduleGpsPoll(resolveGpsSampleIntervalMs());
  }, delayMs);
}

function refreshGpsPollingCadence() {
  if (!gpsPollingStarted) {
    return;
  }

  scheduleGpsPoll(resolveGpsSampleIntervalMs());
}

function startGpsPolling() {
  if (!gpsState.supported || gpsPollingStarted || heatDebugState.gpsPaused) {
    return;
  }

  gpsPollingStarted = true;
  requestGpsSample(true);
  scheduleGpsPoll(resolveGpsSampleIntervalMs());
}

function setFocusModeActive(active) {
  focusModeState.pendingTarget = null;
  focusModeState.pendingSince = 0;

  if (focusModeActive === active) {
    return;
  }

  focusModeActive = active;
  document.body.classList.toggle('mode-focus', focusModeActive);

  if (focusModeActive && settingsOpen) {
    setSettingsOpen(false);
  }

  refreshGpsPollingCadence();
}

function setSlowCruiseModeActive(active) {
  if (slowCruiseModeActive === active) {
    return;
  }

  slowCruiseModeActive = active;
  document.body.classList.toggle('mode-cruise', slowCruiseModeActive);
}

function resolveFocusIntent(next) {
  const motionIntent = clamp((motionState.variationTarget - 0.08) / 0.28, 0, 1);

  if (focusModeActive) {
    return next.spd > focusModeExitSpeedKph ||
      next.rpm > focusModeExitRpm ||
      motionIntent > 0.16;
  }

  return next.spd >= focusModeEnterSpeedKph ||
    (next.spd >= 22 && next.rpm >= focusModeEnterRpm && motionIntent > 0.18);
}

function updateFocusMode(next, now = performance.now()) {
  const desiredFocusMode = resolveFocusIntent(next);

  if (desiredFocusMode === focusModeActive) {
    focusModeState.pendingTarget = null;
    focusModeState.pendingSince = 0;
    return;
  }

  if (focusModeState.pendingTarget !== desiredFocusMode) {
    focusModeState.pendingTarget = desiredFocusMode;
    focusModeState.pendingSince = now;
    return;
  }

  if (now - focusModeState.pendingSince < focusModeCommitDelayMs) {
    return;
  }

  setFocusModeActive(desiredFocusMode);
}

function updateMotionState(next, now = performance.now()) {
  updateFocusMode(next, now);

  if (motionState.lastSampleAt === 0) {
    motionState.lastSampleAt = now;
    motionState.lastSpeed = next.spd;
    motionState.lastAccel = next.acc;
    return;
  }

  const deltaMs = Math.max(16, now - motionState.lastSampleAt);
  const speedRate = Math.abs(next.spd - motionState.lastSpeed) / (deltaMs / 1000);
  const accelDelta = Math.abs(next.acc - motionState.lastAccel);
  const variationSample = clamp((speedRate / 18) + (accelDelta / 280), 0, 1);

  motionState.variationTarget = lerp(motionState.variationTarget, variationSample, 0.24);
  motionState.lastSampleAt = now;
  motionState.lastSpeed = next.spd;
  motionState.lastAccel = next.acc;
}

function normalizeSnapshot(next, now = performance.now()) {
  const snapshot = { ...targetState, ...next };

  if (Boolean(snapshot.fresh)) {
    Object.assign(lastFreshState, snapshot);
    lastFreshTelemetryAt = now;
    return snapshot;
  }

  if (!snapshot.wifi) {
    return snapshot;
  }

  if (lastFreshTelemetryAt === 0 || now - lastFreshTelemetryAt > telemetryHoldMs) {
    return snapshot;
  }

  const heldSnapshot = { ...snapshot };
  for (const key of retainedTelemetryKeys) {
    heldSnapshot[key] = lastFreshState[key];
  }

  return heldSnapshot;
}

function applyDisconnectedSnapshot() {
  lastFreshTelemetryAt = 0;
  motionState.lastSampleAt = 0;
  motionState.lastSpeed = 0;
  motionState.lastAccel = 0;
  motionState.variationTarget = 0;
  motionState.variationDisplay = 0;
  displayedState.spd = 0;
  displayedState.rpm = 0;
  displayedState.acc = 0;
  setFocusModeActive(false);
  setSlowCruiseModeActive(false);
  applySnapshot({
    spd: 0,
    rpm: 0,
    acc: 0,
    wifi: 0,
    obd: 0,
    fresh: 0,
  });
}

function applySnapshot(next) {
  const now = performance.now();
  const normalized = normalizeSnapshot(next, now);

  if (!normalized.wifi) {
    lastFreshTelemetryAt = 0;
  }

  Object.assign(targetState, normalized);
  updateMotionState(normalized, now);
  if (shouldCommitSlowState(normalized)) {
    Object.assign(slowState, normalized);
    lastSlowCommitAt = now;
  }

  scheduleAnimationLoop();
}

function easeInOutSine(value) {
  return 0.5 - (Math.cos(Math.PI * value) / 2);
}

function sampleSimulationSpeed(phaseSeconds) {
  if (phaseSeconds < 3) {
    return 0;
  }

  if (phaseSeconds < 8) {
    return easeInOutSine((phaseSeconds - 3) / 5) * 26;
  }

  if (phaseSeconds < 14) {
    const localPhase = phaseSeconds - 8;
    return 26 + Math.sin(localPhase * 0.72) * 1.4;
  }

  if (phaseSeconds < 20) {
    return 26 + easeInOutSine((phaseSeconds - 14) / 6) * 44;
  }

  if (phaseSeconds < 25) {
    const localPhase = phaseSeconds - 20;
    return 70 + Math.sin(localPhase * 0.64) * 1.6;
  }

  if (phaseSeconds < 31) {
    return 70 - easeInOutSine((phaseSeconds - 25) / 6) * 58;
  }

  if (phaseSeconds < 35) {
    return 12 - easeInOutSine((phaseSeconds - 31) / 4) * 12;
  }

  if (phaseSeconds < 39) {
    return 0;
  }

  if (phaseSeconds < 44) {
    return easeInOutSine((phaseSeconds - 39) / 5) * 24;
  }

  if (phaseSeconds < 48) {
    const localPhase = phaseSeconds - 44;
    return 24 + Math.sin(localPhase * 0.9) * 1.1;
  }

  if (phaseSeconds < 54) {
    return 24 - easeInOutSine((phaseSeconds - 48) / 6) * 24;
  }

  return 0;
}

function nextSimulationSnapshot(deltaMs = 125) {
  simulationSequence += 1;
  simulationPhase += deltaMs / 1000;
  simulationUptimeMs += deltaMs;

  const phaseSeconds = simulationPhase % simulationCycleSeconds;
  const sweep = (Math.sin(simulationPhase * 0.28) + 1) / 2;
  const speed = Math.round(sampleSimulationSpeed(phaseSeconds));
  const speedDelta = simulationSequence === 1 ? 0 : speed - motionState.lastSpeed;
  const accel = deltaMs <= 0
    ? 0
    : clamp(Math.round((speedDelta / (deltaMs / 1000)) * 18), -220, 220);
  const positiveLoad = Math.max(accel, 0);
  const rpm = clamp(
    Math.round(780 + speed * 17 + positiveLoad * 1.15 + Math.sin(simulationPhase * 1.4) * 54),
    760,
    3400,
  );
  const coolant = clamp(Math.round(80 + sweep * 10), 78, 94);
  const weatherTemp = clamp(Math.round(22 + Math.sin(simulationPhase * 0.12) * 7), 14, 33);
  const battery = clamp(Math.round(12420 + Math.cos(simulationPhase * 1.3) * 180), 12100, 12780);
  const fuel = clamp(Math.round(82 - (simulationSequence / 1800) % 58), 24, 82);
  const headlights = Math.sin(simulationPhase * 0.16) > 0.45;
  const weatherCycle = Math.floor((simulationSequence / 90) % 6);
  const weatherCode = [0, 1, 2, 3, 61, 95][weatherCycle];
  const efficiencyFactor = clamp(1.02 - speed / 260, 0.72, 1.02);

  return {
    v: 1,
    seq: simulationSequence,
    up: Math.round(simulationUptimeMs),
    spd: speed,
    rpm,
    clt: coolant,
    wt: weatherTemp,
    wc: weatherCode,
    bat: battery,
    fuel,
    rng: Math.round(fuel * 4.6 * efficiencyFactor),
    acc: accel,
    wifi: 1,
    hl: headlights ? 1 : 0,
    obd: 1,
    fresh: 1,
  };
}

function updateSimulationUi() {
  elements.simulationButton.textContent = simulationMode ? 'Simulation On' : 'Simulation Off';
  elements.simulationStatus.textContent = simulationMode
    ? 'Using browser demo telemetry.'
    : 'Use live gateway telemetry.';
  elements.connectButton.disabled = simulationMode;
  elements.resetButton.disabled = simulationMode;
  elements.wsUrl.disabled = simulationMode;

  if (simulationMode) {
    setStatusIcon(elements.socketStatus, 'Simulation mode', 'live');
  }
}

function setSimulationMode(enabled) {
  simulationMode = enabled;
  lastSimulationTickAt = 0;

  if (enabled) {
    simulationSequence = 0;
    simulationPhase = 0;
    simulationUptimeMs = 0;
    motionState.lastSampleAt = 0;
    motionState.lastSpeed = 0;
    motionState.lastAccel = 0;
    motionState.variationTarget = 0.7;
    motionState.variationDisplay = 0.7;
    localStorage.setItem('driveOrbitSimulation', '1');
    window.clearTimeout(reconnectTimer);
    reconnectAttempt = 0;
    if (socket) {
      const nextSocket = socket;
      socket = null;
      nextSocket.close();
    }
    applySnapshot(nextSimulationSnapshot(125));
  } else {
    localStorage.removeItem('driveOrbitSimulation');
    connectSocket();
  }

  updateSimulationUi();
}

function tickSimulation(now = performance.now()) {
  if (!simulationMode) {
    return;
  }

  if (lastSimulationTickAt !== 0 && now - lastSimulationTickAt < 125) {
    return;
  }

  const deltaMs = lastSimulationTickAt === 0 ? 125 : clamp(now - lastSimulationTickAt, 16, 250);
  lastSimulationTickAt = now;
  applySnapshot(nextSimulationSnapshot(deltaMs));
}

function clearCacheAndReload() {
  elements.cacheStatus.textContent = 'Reloading latest dashboard...';
  elements.cacheResetButton.disabled = true;
  const nextUrl = new URL(window.location.href);
  nextUrl.searchParams.set('refresh', String(Date.now()));
  window.location.replace(nextUrl.toString());
}

function buildSmartNotes() {
  const notes = [];
  const batteryLabel = resolveBatteryLabel(slowState.bat);
  const gpsNote = buildGpsNote();

  if (targetState.obd && targetState.wifi) {
    notes.push(`Live ${Math.round(displayedState.spd)} km/h`);
  }

  notes.push(`Range ${Math.round(slowState.rng)} km · Fuel ${Math.round(slowState.fuel)}%`);
  notes.push(`Battery ${batteryLabel}`);
  notes.push(`Coolant ${resolveCoolantLabel(slowState.clt)}`);

  if (gpsNote) {
    notes.push(gpsNote);
  }

  if (targetState.hl) {
    notes.push('Headlights on');
  }

  if (!targetState.wifi) {
    notes.push('Wi-Fi offline · using cached browser shell only');
  }

  if (!targetState.obd) {
    notes.push('OBD link is not live yet');
  }

  return notes;
}

function updateSmartNote(now = performance.now()) {
  const notes = buildSmartNotes();
  if (notes.length === 0) {
    elements.smartNote.textContent = 'Waiting for telemetry';
    return null;
  }

  if (lastNoteAt === 0 || now - lastNoteAt >= smartNoteIntervalMs) {
    noteIndex = (lastNoteAt === 0) ? 0 : (noteIndex + 1) % notes.length;
    lastNoteAt = now;
  }

  elements.smartNote.textContent = notes[noteIndex % notes.length];

  if (focusModeActive || notes.length < 2) {
    return null;
  }

  return Math.max(0, smartNoteIntervalMs - (now - lastNoteAt));
}

function setStatusIcon(element, label, variant) {
  element.className = 'status-icon';
  if (variant) {
    element.classList.add(`status-icon--${variant}`);
  }
  element.setAttribute('aria-label', label);
  element.title = label;
}

function setBinaryStatus(element, connected, liveLabel, offLabel) {
  setStatusIcon(element, connected ? liveLabel : offLabel, connected ? 'live' : 'off');
}

function shouldCommitSlowState(next) {
  return lastSlowCommitAt === 0 ||
    (performance.now() - lastSlowCommitAt >= slowTelemetryIntervalMs) ||
    next.wifi !== slowState.wifi ||
    next.obd !== slowState.obd ||
    next.fresh !== slowState.fresh ||
    next.fuel !== slowState.fuel ||
    next.rng !== slowState.rng ||
    next.clt !== slowState.clt ||
    next.bat !== slowState.bat ||
    next.wt !== slowState.wt ||
    next.wc !== slowState.wc;
}

function setSocketStatus(text, variant) {
  const label = (() => {
    if (variant === 'live') {
      return 'Gateway link live';
    }
    if (variant === 'pending') {
      return 'Gateway link connecting';
    }
    if (text === 'Retry') {
      return 'Gateway link reconnecting';
    }
    return 'Gateway link error';
  })();

  setStatusIcon(elements.socketStatus, label, variant);
}

function setFreshness(text, variant) {
  setStatusIcon(elements.freshnessStatus, `Telemetry ${text.toLowerCase()}`, variant);
}

function setDashboardTheme(headlightsOn) {
  const theme = headlightsOn ? 'dark' : 'light';
  if (document.body.dataset.theme !== theme) {
    document.body.dataset.theme = theme;
  }

  elements.themeColor?.setAttribute('content', headlightsOn ? '#02060d' : '#0e1828');
}

function setHeadlightIndicator(headlightsOn) {
  const label = headlightsOn ? 'Headlights on' : 'Headlights off';
  elements.lightsValue.dataset.state = headlightsOn ? 'active' : 'idle';
  elements.lightsValue.setAttribute('aria-label', label);
  elements.lightsValue.title = label;
}

function setAccelMeter(accelMg) {
  const ratio = clamp((typeof accelMg === 'number' ? accelMg : 0) / 600, -1, 1);
  elements.accelMarker.style.left = `${50 + ratio * 46}%`;
}

function scheduleReconnect() {
  if (simulationMode) {
    return;
  }

  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  window.clearTimeout(reconnectTimer);
  reconnectAttempt += 1;
  const delay = clamp(500 * 2 ** (reconnectAttempt - 1), 500, 8000);
  reconnectTimer = window.setTimeout(connectSocket, delay);
}

function setSettingsOpen(open) {
  settingsOpen = open;
  elements.settingsPanel.hidden = !open;
  elements.settingsBackdrop.hidden = !open;
  elements.settingsPanel.classList.toggle('is-open', open);
  elements.settingsBackdrop.classList.toggle('is-open', open);
  elements.settingsToggle.setAttribute('aria-expanded', String(open));

  if (open) {
    window.setTimeout(() => elements.wsUrl.focus(), 0);
  } else {
    elements.settingsToggle.focus();
  }
}

function connectSocket() {
  if (simulationMode) {
    updateSimulationUi();
    return;
  }

  window.clearTimeout(reconnectTimer);
  currentSocketUrl = elements.wsUrl.value.trim() || resolveInitialSocketUrl();
  elements.wsUrl.value = currentSocketUrl;
  localStorage.setItem('driveOrbitWsUrl', currentSocketUrl);

  setSocketStatus('Link', 'pending');
  const previousSocket = socket;
  const nextSocket = new WebSocket(currentSocketUrl);
  socket = nextSocket;

  if (previousSocket && previousSocket !== nextSocket) {
    previousSocket.close();
  }

  nextSocket.addEventListener('open', () => {
    if (socket !== nextSocket) {
      return;
    }

    reconnectAttempt = 0;
    window.clearTimeout(reconnectTimer);
    setSocketStatus('Link', 'live');
    setSettingsOpen(false);
  });

  nextSocket.addEventListener('message', (event) => {
    if (socket !== nextSocket) {
      return;
    }

    recordSocketMessage();
    const next = JSON.parse(event.data);
    applySnapshot(next);
  });

  nextSocket.addEventListener('close', () => {
    if (socket !== nextSocket) {
      return;
    }

    socket = null;
    if (simulationMode) {
      return;
    }

    applyDisconnectedSnapshot();
    setSocketStatus('Retry', 'error');
    scheduleReconnect();
  });

  nextSocket.addEventListener('error', () => {
    if (socket !== nextSocket) {
      return;
    }

    setSocketStatus('Error', 'error');
  });
}

function drawGlowDot(x, y, radius, fillColor, glowColor, glowBlur) {
  gaugeContext.save();
  gaugeContext.fillStyle = fillColor;
  gaugeContext.shadowBlur = glowBlur;
  gaugeContext.shadowColor = glowColor;
  gaugeContext.beginPath();
  gaugeContext.arc(x, y, radius, 0, Math.PI * 2);
  gaugeContext.fill();
  gaugeContext.restore();
}

function mapRpmToGaugeRatio(rpm) {
  const safeRpm = clamp(rpm, 0, maxGaugeRpm);

  if (safeRpm <= rpmPrimaryBandCeiling) {
    return (safeRpm / rpmPrimaryBandCeiling) * rpmPrimaryBandRatio;
  }

  return rpmPrimaryBandRatio +
    ((safeRpm - rpmPrimaryBandCeiling) / (maxGaugeRpm - rpmPrimaryBandCeiling)) * (1 - rpmPrimaryBandRatio);
}

function resolveCurveBend() {
  const baseBend = focusModeActive ? 0.16 : 0.22;
  return clamp(baseBend + motionState.variationDisplay * 0.06, baseBend, baseBend + 0.08);
}

function resolveSlowCruiseEmphasis(speedKph = displayedState.spd) {
  if (!focusModeActive) {
    return 0;
  }

  const speedPresence = clamp((speedKph - focusModeExitSpeedKph) / 8, 0, 1);
  const lowSpeedPreference = 1 - clamp((speedKph - 38) / 18, 0, 1);
  const stability = 1 - clamp((motionState.variationDisplay - 0.09) / 0.22, 0, 1);
  return clamp(speedPresence * lowSpeedPreference * stability, 0, 1);
}

function syncSlowCruiseMode() {
  const emphasis = resolveSlowCruiseEmphasis();
  const nextSlowCruiseMode = slowCruiseModeActive
    ? emphasis > slowCruiseExitEmphasis
    : emphasis >= slowCruiseEnterEmphasis;

  setSlowCruiseModeActive(nextSlowCruiseMode);

  return emphasis;
}

function getQuadraticPoint(start, control, end, t) {
  const inverse = 1 - t;
  return {
    x: (inverse * inverse * start.x) + (2 * inverse * t * control.x) + (t * t * end.x),
    y: (inverse * inverse * start.y) + (2 * inverse * t * control.y) + (t * t * end.y),
  };
}

function getQuadraticTangent(start, control, end, t) {
  return {
    x: (2 * (1 - t) * (control.x - start.x)) + (2 * t * (end.x - control.x)),
    y: (2 * (1 - t) * (control.y - start.y)) + (2 * t * (end.y - control.y)),
  };
}

function traceCurveSegment(curve, startT, endT) {
  const safeEnd = clamp(endT, startT, 1);
  const steps = Math.max(18, Math.round((safeEnd - startT) * 72));
  const firstPoint = getQuadraticPoint(curve.start, curve.control, curve.end, startT);

  gaugeContext.beginPath();
  gaugeContext.moveTo(firstPoint.x, firstPoint.y);

  for (let index = 1; index <= steps; index += 1) {
    const ratio = index / steps;
    const point = getQuadraticPoint(curve.start, curve.control, curve.end, startT + (safeEnd - startT) * ratio);
    gaugeContext.lineTo(point.x, point.y);
  }
}

function drawCurveStroke(curve, startT, endT, color, width) {
  if (endT <= startT) {
    return;
  }

  gaugeContext.strokeStyle = color;
  gaugeContext.lineWidth = width;
  gaugeContext.lineCap = 'round';
  traceCurveSegment(curve, startT, endT);
  gaugeContext.stroke();
}

function drawGlowCurve(curve, startT, endT, color, width, glowColor, glowBlur) {
  gaugeContext.save();
  gaugeContext.shadowBlur = glowBlur;
  gaugeContext.shadowColor = glowColor;
  drawCurveStroke(curve, startT, endT, color, width);
  gaugeContext.restore();
}

function drawCurveTicks(curve, count, color, width, tickLength) {
  gaugeContext.save();
  gaugeContext.strokeStyle = color;
  gaugeContext.lineWidth = width;
  gaugeContext.lineCap = 'round';

  for (let index = 0; index <= count; index += 1) {
    const t = index / count;
    const point = getQuadraticPoint(curve.start, curve.control, curve.end, t);
    const tangent = getQuadraticTangent(curve.start, curve.control, curve.end, t);
    const normalLength = Math.hypot(tangent.y, tangent.x) || 1;
    const normalX = tangent.y / normalLength;
    const normalY = -tangent.x / normalLength;

    gaugeContext.beginPath();
    gaugeContext.moveTo(point.x - normalX * 1.5, point.y - normalY * 1.5);
    gaugeContext.lineTo(point.x + normalX * tickLength, point.y + normalY * tickLength);
    gaugeContext.stroke();
  }

  gaugeContext.restore();
}

function drawNeedle(anchorX, anchorY, tipX, tipY, color, width, glowColor = color, glowBlur = 18) {
  const deltaX = tipX - anchorX;
  const deltaY = tipY - anchorY;

  gaugeContext.save();
  gaugeContext.shadowBlur = glowBlur;
  gaugeContext.shadowColor = glowColor;
  gaugeContext.strokeStyle = color;
  gaugeContext.lineWidth = width;
  gaugeContext.lineCap = 'round';
  gaugeContext.beginPath();
  gaugeContext.moveTo(anchorX - deltaX * 0.08, anchorY - deltaY * 0.08);
  gaugeContext.lineTo(anchorX + deltaX * 0.98, anchorY + deltaY * 0.98);
  gaugeContext.stroke();
  gaugeContext.restore();
}

function resolveGaugeCurve(width, height, compactGauge, curveBend) {
  const centerX = width / 2;
  const crownY = height * (compactGauge ? 0.27 : 0.25);
  const spanX = Math.min(width * (compactGauge ? 0.34 : 0.36), (width / 2) - (compactGauge ? 18 : 30));
  const sideDrop = (compactGauge ? 18 : 24) + curveBend * (compactGauge ? 10 : 14);
  const controlLift = (compactGauge ? 26 : 34) + curveBend * (compactGauge ? 10 : 14);

  return {
    start: { x: centerX - spanX, y: crownY + sideDrop },
    control: { x: centerX, y: crownY - controlLift },
    end: { x: centerX + spanX, y: crownY + sideDrop },
    anchor: {
      x: centerX,
      y: height * (compactGauge ? 0.72 : 0.74),
    },
    outerRadius: Math.min(width * 0.44, height * 0.44),
    strokeWidth: compactGauge ? 28 : 34,
    tickCount: compactGauge ? 6 : 7,
    tickLength: compactGauge ? 8 : 10,
    tickWidth: compactGauge ? 1.2 : 1.4,
    glowBlur: compactGauge ? 12 : 16,
  };
}

function drawGauge() {
  recordGaugeDraw();
  resizeGaugeCanvas();
  const width = elements.gaugeCanvas.width;
  const height = elements.gaugeCanvas.height;
  const compactGauge = width <= 540 || height <= 320;
  const curveBend = resolveCurveBend();
  const curve = resolveGaugeCurve(width, height, compactGauge, curveBend);
  const slowCruiseEmphasis = resolveSlowCruiseEmphasis(displayedState.spd);
  const rpmRatio = mapRpmToGaugeRatio(displayedState.rpm);
  const activeT = clamp(rpmRatio, 0, 1);
  const trackWidth = lerp(curve.strokeWidth, compactGauge ? 18 : 22, slowCruiseEmphasis);
  const trackGlowBlur = lerp(curve.glowBlur, compactGauge ? 8 : 10, slowCruiseEmphasis);
  const tickLength = lerp(curve.tickLength, compactGauge ? 5 : 6, slowCruiseEmphasis);
  const tickWidth = lerp(curve.tickWidth, compactGauge ? 1 : 1.1, slowCruiseEmphasis);
  const dotRadius = lerp(compactGauge ? 4.5 : 5.5, compactGauge ? 3.2 : 4, slowCruiseEmphasis);

  gaugeContext.clearRect(0, 0, width, height);
  gaugeContext.fillStyle = 'rgba(255,255,255,0.015)';
  gaugeContext.fillRect(0, 0, width, height);

  const haloGradient = gaugeContext.createRadialGradient(
    width / 2,
    curve.control.y + curve.outerRadius * 0.06,
    curve.outerRadius * 0.08,
    width / 2,
    curve.anchor.y - curve.outerRadius * 0.24,
    curve.outerRadius * 1.12,
  );
  haloGradient.addColorStop(0, 'rgba(132, 255, 243, 0.1)');
  haloGradient.addColorStop(0.24, 'rgba(95, 214, 255, 0.08)');
  haloGradient.addColorStop(0.56, 'rgba(62, 118, 255, 0.04)');
  haloGradient.addColorStop(1, 'rgba(113, 207, 255, 0)');
  gaugeContext.fillStyle = haloGradient;
  gaugeContext.fillRect(0, 0, width, height);

  const trackGradient = gaugeContext.createLinearGradient(
    curve.start.x,
    curve.control.y,
    curve.end.x,
    curve.start.y,
  );
  trackGradient.addColorStop(0, 'rgba(29, 45, 70, 0.92)');
  trackGradient.addColorStop(0.55, 'rgba(44, 74, 114, 0.84)');
  trackGradient.addColorStop(1, 'rgba(69, 108, 152, 0.68)');

  const activeGradient = gaugeContext.createLinearGradient(
    curve.start.x,
    curve.control.y - curve.outerRadius * 0.1,
    curve.end.x,
    curve.start.y,
  );
  activeGradient.addColorStop(0, '#2d7fff');
  activeGradient.addColorStop(0.48, '#52d7ff');
  activeGradient.addColorStop(1, '#a7fff6');

  const redlineGradient = gaugeContext.createLinearGradient(
    curve.start.x,
    curve.control.y,
    curve.end.x,
    curve.start.y,
  );
  redlineGradient.addColorStop(0, '#ffbf70');
  redlineGradient.addColorStop(1, '#ff7f5d');

  drawCurveStroke(curve, 0, 1, 'rgba(255,255,255,0.02)', 1.4);
  drawCurveTicks(
    curve,
    curve.tickCount,
    `rgba(184, 224, 255, ${lerp(0.12, 0.08, slowCruiseEmphasis)})`,
    tickWidth,
    tickLength,
  );

  drawGlowCurve(curve, 0, 1, 'rgba(3, 9, 16, 0.86)', trackWidth + lerp(4, 2, slowCruiseEmphasis), 'rgba(0, 0, 0, 0.16)', compactGauge ? 8 : 12);
  drawCurveStroke(curve, 0, 1, trackGradient, trackWidth);
  drawGlowCurve(curve, 0, activeT, activeGradient, trackWidth, `rgba(95, 220, 255, ${lerp(0.22, 0.16, slowCruiseEmphasis)})`, trackGlowBlur);
  drawCurveStroke(curve, 0, activeT, 'rgba(245, 252, 255, 0.42)', Math.max(1.5, trackWidth * 0.08));

  if (activeT > 0.84) {
    drawGlowCurve(
      curve,
      0.84,
      activeT,
      redlineGradient,
      Math.max(trackWidth * 0.26, compactGauge ? 6 : 8),
      'rgba(255, 145, 96, 0.2)',
      compactGauge ? 8 : 10,
    );
  }

  const activePoint = getQuadraticPoint(curve.start, curve.control, curve.end, activeT);
  drawGlowDot(activePoint.x, activePoint.y, dotRadius, '#f2feff', 'rgba(124, 236, 255, 0.28)', compactGauge ? 10 : 12);
}

function resolveSpeedPhaseLabel() {
  if (!targetState.wifi) {
    return 'Offline';
  }

  if (!targetState.obd) {
    return 'Linking';
  }

  if (focusModeActive) {
    return slowCruiseModeActive ? 'Cruise Focus' : 'Drive Focus';
  }

  return 'Cruise Ready';
}

function updateDom(now = performance.now()) {
  recordDomUpdate();
  elements.speedValue.textContent = Math.round(displayedState.spd);
  elements.speedPhase.textContent = resolveSpeedPhaseLabel();
  if (elements.rpmValue) {
    elements.rpmValue.textContent = Math.round(displayedState.rpm).toLocaleString();
  }
  elements.accelValue.textContent = resolveAccelLabel(displayedState.acc);
  setAccelMeter(displayedState.acc);
  setHeadlightIndicator(Boolean(targetState.hl));
  elements.gpsValue.textContent = resolveGpsInlineLabel();
  elements.gpsValue.dataset.state = gpsState.status === 'ready' ? 'ok' : 'idle';
  elements.fuelValue.textContent = `${Math.round(slowState.fuel)}%`;
  elements.rangeValue.textContent = `${Math.round(slowState.rng)} km`;
  elements.focusFuelValue.textContent = `${Math.round(slowState.fuel)}%`;
  elements.focusRangeValue.textContent = `${Math.round(slowState.rng)} km`;
  elements.weatherValue.textContent = resolveWeatherLabel(slowState.wc, slowState.wt);
  setDashboardTheme(Boolean(targetState.hl));
  setBinaryStatus(elements.wifiStatus, Boolean(targetState.wifi), 'Wi-Fi live', 'Wi-Fi off');
  setBinaryStatus(elements.obdStatus, Boolean(targetState.obd), 'OBD live', 'OBD off');
  elements.fuelFill.style.transform = `scaleX(${clamp(slowState.fuel / 100, 0, 1)})`;
  return updateSmartNote(now);
}

function animate(now = performance.now()) {
  const frameStartedAt = performance.now();
  tickSimulation(now);

  const variationLerp = motionState.variationTarget > motionState.variationDisplay ? 0.08 : 0.014;
  motionState.variationDisplay = settleValue(
    motionState.variationDisplay,
    motionState.variationTarget,
    variationLerp,
    renderEpsilon.variation,
  );

  displayedState.spd = targetState.spd;
  displayedState.rpm = settleValue(displayedState.rpm, targetState.rpm, focusModeActive ? 0.72 : 0.8, renderEpsilon.rpm);
  displayedState.clt = settleValue(displayedState.clt, slowState.clt, 0.2, renderEpsilon.clt);
  displayedState.bat = settleValue(displayedState.bat, slowState.bat, 0.2, renderEpsilon.bat);
  displayedState.fuel = settleValue(displayedState.fuel, slowState.fuel, 0.12, renderEpsilon.fuel);
  displayedState.rng = settleValue(displayedState.rng, slowState.rng, 0.2, renderEpsilon.rng);
  displayedState.acc = settleValue(displayedState.acc, targetState.acc, focusModeActive ? 0.42 : 0.56, renderEpsilon.acc);
  displayedState.wifi = targetState.wifi;
  displayedState.obd = targetState.obd;
  displayedState.hl = targetState.hl;
  syncSlowCruiseMode();

  const freshnessState = resolveFreshnessState(now);
  setFreshness(freshnessState.text, freshnessState.variant);

  const nextNoteDelayMs = updateDom(now);
  drawGauge();
  recordFrameMetrics(performance.now() - frameStartedAt);

  const nextRenderDelayMs = resolveNextRenderDelay(now, nextNoteDelayMs, freshnessState.nextWakeDelayMs);
  if (nextRenderDelayMs != null) {
    scheduleRenderWake(nextRenderDelayMs);
  }
}

elements.wsUrl.value = currentSocketUrl;
elements.settingsToggle.addEventListener('click', () => {
  setSettingsOpen(!settingsOpen);
});
elements.settingsClose.addEventListener('click', () => {
  setSettingsOpen(false);
});
elements.settingsBackdrop.addEventListener('click', () => {
  setSettingsOpen(false);
});
elements.connectButton.addEventListener('click', connectSocket);
elements.resetButton.addEventListener('click', () => {
  localStorage.removeItem('driveOrbitWsUrl');
  elements.wsUrl.value = resolveDefaultSocketUrl();
  connectSocket();
  setSettingsOpen(false);
});
elements.simulationButton.addEventListener('click', () => {
  setSimulationMode(!simulationMode);
});
elements.cacheResetButton.addEventListener('click', () => {
  clearCacheAndReload();
});
elements.heatDebugToggle.addEventListener('click', () => {
  setHeatDebugEnabled(!heatDebugState.enabled);
});
elements.heatDebugClose.addEventListener('click', () => {
  setHeatDebugEnabled(false);
});
elements.heatRenderToggle.addEventListener('click', () => {
  setRenderPaused(!heatDebugState.renderPaused);
});
elements.heatGpsToggle.addEventListener('click', () => {
  setGpsPaused(!heatDebugState.gpsPaused);
});

document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && settingsOpen) {
    setSettingsOpen(false);
  }
});

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') {
    cancelAnimationLoop();
    window.clearTimeout(gpsPollTimer);
    gpsPollTimer = null;
    return;
  }

  if (!simulationMode && document.visibilityState === 'visible' && socket?.readyState !== WebSocket.OPEN) {
    connectSocket();
  }

  if (document.visibilityState === 'visible') {
    requestGpsSample(false);
    refreshGpsPollingCadence();
    scheduleAnimationLoop();
  }
});

window.addEventListener('resize', () => {
  resizeGaugeCanvas();
  scheduleAnimationLoop();
});
window.addEventListener('orientationchange', () => {
  resizeGaugeCanvas();
  scheduleAnimationLoop();
});
window.visualViewport?.addEventListener('resize', () => {
  resizeGaugeCanvas();
  scheduleAnimationLoop();
});

resizeGaugeCanvas();
updateRuntimeUi();
updateSimulationUi();
setDashboardTheme(Boolean(targetState.hl));
if (heatDebugState.enabled) {
  startHeatDebugSampling();
}
refreshHeatDebugUi();
startGpsPolling();
if (simulationMode) {
  applySnapshot(nextSimulationSnapshot(125));
} else {
  connectSocket();
}
scheduleAnimationLoop();