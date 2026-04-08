// ── Elements ──

const el = {
  speedValue: document.getElementById('speed-value'),
  rpmValue: document.getElementById('rpm-value'),
  gearValue: document.getElementById('gear-value'),
  fuelValue: document.getElementById('fuel-value'),
  fuelFill: document.getElementById('fuel-fill'),
  rangeValue: document.getElementById('range-value'),
  tripValue: document.getElementById('trip-value'),
  waveform: document.getElementById('waveform'),
  statusDot: document.getElementById('status-dot'),
  settingsToggle: document.getElementById('settings-toggle'),
  settingsPanel: document.getElementById('settings-panel'),
  settingsBackdrop: document.getElementById('settings-backdrop'),
  settingsClose: document.getElementById('settings-close'),
  wsUrl: document.getElementById('ws-url'),
  resetButton: document.getElementById('reset-button'),
  simulationButton: document.getElementById('simulation-button'),
  simulationStatus: document.getElementById('simulation-status'),
  fuelCluster: document.querySelector('.fuel-cluster'),
  infoBar: document.querySelector('.info-bar'),
  // Trip sheet
  tripSheetBackdrop: document.getElementById('trip-sheet-backdrop'),
  tripSheet: document.getElementById('trip-sheet'),
  tripResetButton: document.getElementById('trip-reset-button'),
  tsDuration: document.getElementById('ts-duration'),
  tsDistance: document.getElementById('ts-distance'),
  tsAvgSpeed: document.getElementById('ts-avg-speed'),
  tsMaxSpeed: document.getElementById('ts-max-speed'),
  tsScoreNum: document.getElementById('ts-score-num'),
  tsScoreGrade: document.getElementById('ts-score-grade'),
  tsScoreLabel: document.getElementById('ts-score-label'),
  tsBarSmoothness: document.getElementById('ts-bar-smoothness'),
  tsBarConsistency: document.getElementById('ts-bar-consistency'),
  tsEvAccel: document.getElementById('ts-ev-accel'),
  tsEvBrake: document.getElementById('ts-ev-brake'),
  tsEvOverspeed: document.getElementById('ts-ev-overspeed'),
};

// ── Constants ──

const WAVEFORM_BARS = 60;
const MAX_RPM = 6000;
const SIMULATION_CYCLE_S = 56;

// ── State ──

let socket = null;
let reconnectTimer = null;
let reconnectAttempt = 0;
let settingsOpen = false;
let simulationMode = false;
let simulationTimer = null;
let simulationSeq = 0;
let simulationPhase = 0;
let simulationUptimeMs = 0;
let tripDistanceKm = 0;
let lastTickAt = 0;

// ── Trip Metrics State ──

let tripStartTime = Date.now();
let tripSpeedSum = 0;
let tripSpeedCount = 0;
let tripMaxSpeed = 0;
let tripHarshAccelCount = 0;
let tripHarshBrakeCount = 0;
let tripOverspeedCount = 0;
const OVERSPEED_KPH = 120;
const HARSH_KPH_PER_500MS = 18;
const HARSH_DEBOUNCE_MS = 2500;
let lastHarshAccelAt = 0;
let lastHarshBrakeAt = 0;
let lastOverspeedAt = 0;
let tripSheetOpen = false;
const speedBuffer = []; // last 5 speed samples for harsh-event window

const rpmHistory = new Array(WAVEFORM_BARS).fill(0);
const waveformBars = [];

// ── Waveform Setup ──

for (let i = 0; i < WAVEFORM_BARS; i++) {
  const bar = document.createElement('div');
  bar.className = 'bar';
  bar.style.height = '2px';
  el.waveform.appendChild(bar);
  waveformBars.push(bar);
}

// ── Utility ──

function clamp(v, lo, hi) {
  return Math.min(hi, Math.max(lo, v));
}

function isNativeShell() {
  return typeof window.Capacitor?.isNativePlatform === 'function' && window.Capacitor.isNativePlatform();
}

function resolveDefaultSocketUrl() {
  if (isNativeShell()) {
    return 'ws://carconsole.local/ws';
  }
  return `ws://${window.location.host}/ws`;
}

function resolveInitialSocketUrl() {
  const params = new URLSearchParams(window.location.search);
  const queryWs = params.get('ws');
  if (queryWs) {
    localStorage.setItem('driveOrbitWsUrl', queryWs);
    return queryWs;
  }
  const saved = localStorage.getItem('driveOrbitWsUrl');
  return saved || resolveDefaultSocketUrl();
}

let currentSocketUrl = resolveInitialSocketUrl();

// ── Gear Estimation ──

function estimateGear(speedKph) {
  if (speedKph < 2) return 'P';
  if (speedKph < 12) return 'D1';
  if (speedKph < 28) return 'D2';
  if (speedKph < 44) return 'D3';
  if (speedKph < 62) return 'D4';
  if (speedKph < 82) return 'D5';
  return 'D6';
}

// ── Trip Metrics ──

function resetTripMetrics() {
  tripStartTime = Date.now();
  tripDistanceKm = 0;
  tripSpeedSum = 0;
  tripSpeedCount = 0;
  tripMaxSpeed = 0;
  tripHarshAccelCount = 0;
  tripHarshBrakeCount = 0;
  tripOverspeedCount = 0;
  lastHarshAccelAt = 0;
  lastHarshBrakeAt = 0;
  lastOverspeedAt = 0;
  speedBuffer.length = 0;
  el.tripValue.textContent = '0.0 km';
}

function updateTripMetrics(speed, now) {
  if (speed > 0) {
    tripSpeedSum += speed;
    tripSpeedCount++;
    if (speed > tripMaxSpeed) tripMaxSpeed = speed;
  }

  // Overspeed tracking (debounced)
  if (speed > OVERSPEED_KPH && now - lastOverspeedAt > HARSH_DEBOUNCE_MS) {
    tripOverspeedCount++;
    lastOverspeedAt = now;
  }

  // Harsh event detection over a 500ms(5-tick) window
  speedBuffer.push(speed);
  if (speedBuffer.length > 5) speedBuffer.shift();

  if (speedBuffer.length === 5) {
    const delta = speedBuffer[4] - speedBuffer[0];
    if (delta > HARSH_KPH_PER_500MS && now - lastHarshAccelAt > HARSH_DEBOUNCE_MS) {
      tripHarshAccelCount++;
      lastHarshAccelAt = now;
    } else if (delta < -HARSH_KPH_PER_500MS && now - lastHarshBrakeAt > HARSH_DEBOUNCE_MS) {
      tripHarshBrakeCount++;
      lastHarshBrakeAt = now;
    }
  }
}

function computeTripScore() {
  let score = 100;
  score -= tripHarshAccelCount * 3;
  score -= tripHarshBrakeCount * 4;
  score -= tripOverspeedCount * 5;
  score = clamp(score, 0, 100);

  let grade, label;
  if (score >= 90) { grade = 'S'; label = 'Silky Smooth'; }
  else if (score >= 75) { grade = 'A'; label = 'Solid Driver'; }
  else if (score >= 60) { grade = 'B'; label = 'Room to Improve'; }
  else if (score >= 40) { grade = 'C'; label = 'Rough Edges'; }
  else { grade = 'D'; label = 'Take it Easy'; }

  const smoothness = clamp(100 - tripHarshAccelCount * 14 - tripHarshBrakeCount * 18, 0, 100);
  const consistency = tripSpeedCount > 0
    ? clamp(Math.round((score * 0.6 + smoothness * 0.4)), 0, 100)
    : 0;

  return { score, grade, label, smoothness, consistency };
}

function formatDuration(ms) {
  const totalSec = Math.floor(ms / 1000);
  const h = Math.floor(totalSec / 3600);
  const m = Math.floor((totalSec % 3600) / 60);
  const s = totalSec % 60;
  if (h > 0) return `${h}:${String(m).padStart(2, '0')}`;
  return `${m}:${String(s).padStart(2, '0')}`;
}

// ── Trip Sheet ──

function gradeClass(grade) {
  if (grade === 'B') return 'grade-b';
  if (grade === 'C' || grade === 'D') return 'grade-cd';
  return '';
}

function populateTripSheet() {
  const elapsed = Date.now() - tripStartTime;
  const avgSpeed = tripSpeedCount > 0 ? Math.round(tripSpeedSum / tripSpeedCount) : 0;
  const { score, grade, label, smoothness, consistency } = computeTripScore();
  const cls = gradeClass(grade);

  el.tsDuration.textContent = formatDuration(elapsed);
  el.tsDistance.textContent = tripDistanceKm.toFixed(1) + ' km';
  el.tsAvgSpeed.textContent = avgSpeed + ' km/h';
  el.tsMaxSpeed.textContent = tripMaxSpeed + ' km/h';

  el.tsScoreNum.textContent = score > 0 ? String(score) : '—';
  el.tsScoreNum.className = 'trip-score-badge' + (cls ? ' ' + cls : '');
  el.tsScoreGrade.textContent = tripSpeedCount > 0 ? '/ ' + grade : '';
  el.tsScoreGrade.className = 'trip-score-grade' + (cls ? ' ' + cls : '');
  el.tsScoreLabel.textContent = tripSpeedCount > 0 ? label : 'No trip data yet';

  el.tsBarSmoothness.style.width = smoothness + '%';
  el.tsBarSmoothness.className = 'trip-bar-fill' + (cls ? ' ' + cls : '');
  el.tsBarConsistency.style.width = consistency + '%';
  el.tsBarConsistency.className = 'trip-bar-fill' + (cls ? ' ' + cls : '');

  el.tsEvAccel.textContent = `${tripHarshAccelCount} harsh acceleration${tripHarshAccelCount !== 1 ? 's' : ''}`;
  el.tsEvBrake.textContent = `${tripHarshBrakeCount} harsh braking${tripHarshBrakeCount !== 1 ? '' : ''}`;
  el.tsEvOverspeed.textContent = `${tripOverspeedCount} over-speed event${tripOverspeedCount !== 1 ? 's' : ''}`;
}

function openTripSheet() {
  if (tripSheetOpen) return;
  populateTripSheet();
  tripSheetOpen = true;
  el.tripSheet.hidden = false;
  el.tripSheetBackdrop.hidden = false;
  // Force reflow before adding class so transition fires
  el.tripSheet.offsetHeight; // eslint-disable-line no-unused-expressions
  el.tripSheetBackdrop.offsetHeight; // eslint-disable-line no-unused-expressions
  el.tripSheet.classList.add('is-open');
  el.tripSheetBackdrop.classList.add('is-open');
  el.tripSheet.setAttribute('aria-hidden', 'false');
}

function closeTripSheet() {
  if (!tripSheetOpen) return;
  tripSheetOpen = false;
  el.tripSheet.classList.remove('is-open');
  el.tripSheetBackdrop.classList.remove('is-open');
  el.tripSheet.setAttribute('aria-hidden', 'true');
  window.setTimeout(() => {
    if (!tripSheetOpen) {
      el.tripSheet.hidden = true;
      el.tripSheetBackdrop.hidden = true;
    }
  }, 340);
}

function setStatusDot(variant, title) {
  el.statusDot.className = 'status-dot' + (variant ? ' ' + variant : '');
  el.statusDot.title = title;
}

// ── Update DOM ──

function applyTelemetry(data) {
  const speed = data.spd ?? 0;
  const rpm = data.rpm ?? 0;
  const fuel = data.fuel ?? 0;
  const range = data.rng ?? 0;
  const now = Date.now();

  // Speed
  el.speedValue.textContent = speed;

  // RPM waveform
  rpmHistory.push(rpm);
  if (rpmHistory.length > WAVEFORM_BARS) rpmHistory.shift();

  for (let i = 0; i < WAVEFORM_BARS; i++) {
    const frac = clamp(rpmHistory[i] / MAX_RPM, 0, 1);
    const h = 2 + frac * 30;
    const bar = waveformBars[i];
    bar.style.height = h + 'px';
    if (frac < 0.5) bar.style.background = '#2dd4bf';
    else if (frac < 0.75) bar.style.background = '#fbbf24';
    else bar.style.background = '#fb7185';
  }

  // RPM readout
  el.rpmValue.textContent = rpm.toLocaleString();

  // Gear
  el.gearValue.textContent = estimateGear(speed);

  // Fuel
  el.fuelValue.textContent = fuel + '%';
  el.fuelFill.style.width = clamp(fuel, 0, 100) + '%';
  el.fuelCluster.classList.toggle('low', fuel > 0 && fuel <= 20);
  el.fuelCluster.classList.toggle('critical', fuel > 0 && fuel <= 10);

  // Range
  el.rangeValue.textContent = range + ' km';

  // Trip (accumulate)
  tripDistanceKm += speed / 36000; // 100ms ticks → /36000 for km
  el.tripValue.textContent = tripDistanceKm.toFixed(1) + ' km';

  // Metrics for trip summary
  updateTripMetrics(speed, now);

  // Live-update sheet if open
  if (tripSheetOpen) populateTripSheet();
}

// ── WebSocket ──

function scheduleReconnect() {
  if (simulationMode) return;
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;

  window.clearTimeout(reconnectTimer);
  reconnectAttempt++;
  const delay = clamp(500 * 2 ** (reconnectAttempt - 1), 500, 8000);
  reconnectTimer = window.setTimeout(connectSocket, delay);
}

function connectSocket() {
  if (simulationMode) return;

  window.clearTimeout(reconnectTimer);
  currentSocketUrl = el.wsUrl.value.trim() || resolveInitialSocketUrl();
  el.wsUrl.value = currentSocketUrl;
  localStorage.setItem('driveOrbitWsUrl', currentSocketUrl);

  setStatusDot('pending', 'Connecting');
  const prev = socket;
  const next = new WebSocket(currentSocketUrl);
  socket = next;

  if (prev && prev !== next) prev.close();

  next.addEventListener('open', () => {
    if (socket !== next) return;
    reconnectAttempt = 0;
    window.clearTimeout(reconnectTimer);
    setStatusDot('live', 'Connected');
    setSettingsOpen(false);
  });

  next.addEventListener('message', (event) => {
    if (socket !== next) return;
    const data = JSON.parse(event.data);
    applyTelemetry(data);
  });

  next.addEventListener('close', () => {
    if (socket !== next) return;
    socket = null;
    if (simulationMode) return;
    setStatusDot('error', 'Disconnected');
    scheduleReconnect();
  });

  next.addEventListener('error', () => {
    if (socket !== next) return;
    setStatusDot('error', 'Connection error');
  });
}

// ── Simulation ──

function easeInOutSine(t) {
  return 0.5 - Math.cos(Math.PI * t) / 2;
}

function sampleSimSpeed(phase) {
  if (phase < 3) return 0;
  if (phase < 8) return easeInOutSine((phase - 3) / 5) * 26;
  if (phase < 14) return 26 + Math.sin((phase - 8) * 0.72) * 1.4;
  if (phase < 20) return 26 + easeInOutSine((phase - 14) / 6) * 44;
  if (phase < 25) return 70 + Math.sin((phase - 20) * 0.64) * 1.6;
  if (phase < 31) return 70 - easeInOutSine((phase - 25) / 6) * 58;
  if (phase < 35) return 12 - easeInOutSine((phase - 31) / 4) * 12;
  if (phase < 39) return 0;
  if (phase < 44) return easeInOutSine((phase - 39) / 5) * 24;
  if (phase < 48) return 24 + Math.sin((phase - 44) * 0.9) * 1.1;
  if (phase < 54) return 24 - easeInOutSine((phase - 48) / 6) * 24;
  return 0;
}

function nextSimSnapshot(deltaMs) {
  simulationSeq++;
  simulationPhase += deltaMs / 1000;
  simulationUptimeMs += deltaMs;

  const phaseS = simulationPhase % SIMULATION_CYCLE_S;
  const speed = Math.round(sampleSimSpeed(phaseS));
  const rpm = clamp(
    Math.round(780 + speed * 17 + Math.max(speed - 40, 0) * 1.1 + Math.sin(simulationPhase * 1.4) * 54),
    760, 3400,
  );
  const fuel = clamp(Math.round(82 - (simulationSeq / 1800) % 58), 24, 82);
  const effFactor = clamp(1.02 - speed / 260, 0.72, 1.02);

  return {
    v: 1,
    seq: simulationSeq,
    spd: speed,
    rpm,
    fuel,
    rng: Math.round(fuel * 4.6 * effFactor),
    obd: 1,
    wifi: 1,
    fresh: 1,
  };
}

function startSimulation() {
  stopSimulation();
  simulationSeq = 0;
  simulationPhase = 0;
  simulationUptimeMs = 0;
  lastTickAt = performance.now();
  setStatusDot('live', 'Simulation');

  simulationTimer = window.setInterval(() => {
    const now = performance.now();
    const dt = now - lastTickAt;
    lastTickAt = now;
    applyTelemetry(nextSimSnapshot(dt));
  }, 100);
}

function stopSimulation() {
  if (simulationTimer) {
    window.clearInterval(simulationTimer);
    simulationTimer = null;
  }
}

function setSimulationMode(enabled) {
  simulationMode = enabled;
  updateSimulationUi();

  if (enabled) {
    if (socket) { socket.close(); socket = null; }
    window.clearTimeout(reconnectTimer);
    startSimulation();
  } else {
    stopSimulation();
    connectSocket();
  }
}

function updateSimulationUi() {
  el.simulationButton.textContent = simulationMode ? 'Simulation On' : 'Simulation Off';
  el.simulationStatus.textContent = simulationMode ? 'Using browser demo telemetry.' : 'Use live gateway telemetry.';
  el.resetButton.disabled = simulationMode;
}

// ── Settings Panel ──

function setSettingsOpen(open) {
  settingsOpen = open;
  el.settingsPanel.hidden = !open;
  el.settingsBackdrop.hidden = !open;
  el.settingsPanel.classList.toggle('is-open', open);
  el.settingsBackdrop.classList.toggle('is-open', open);
  el.settingsToggle.setAttribute('aria-expanded', String(open));

  if (open) {
    window.setTimeout(() => el.resetButton.focus(), 60);
  } else {
    el.settingsToggle.focus();
  }
}

// ── Event Listeners ──

el.settingsToggle.addEventListener('click', () => setSettingsOpen(!settingsOpen));
el.settingsClose.addEventListener('click', () => setSettingsOpen(false));
el.settingsBackdrop.addEventListener('click', () => setSettingsOpen(false));
el.resetButton.addEventListener('click', () => {
  localStorage.removeItem('driveOrbitWsUrl');
  el.wsUrl.value = resolveDefaultSocketUrl();
  connectSocket();
  setSettingsOpen(false);
});

el.simulationButton.addEventListener('click', () => setSimulationMode(!simulationMode));

// Trip sheet — Reset button
el.tripResetButton.addEventListener('click', () => {
  resetTripMetrics();
  populateTripSheet();
});

// Trip sheet — backdrop dismiss
el.tripSheetBackdrop.addEventListener('click', closeTripSheet);

// Trip sheet — Escape key
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') {
    if (tripSheetOpen) { closeTripSheet(); return; }
    if (settingsOpen) setSettingsOpen(false);
  }
});

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') return;
  if (!simulationMode && (!socket || socket.readyState !== WebSocket.OPEN)) {
    connectSocket();
  }
});

// ── Trip Sheet Gestures ──

// Swipe-up on info bar to open sheet
{
  let touchStartY = 0;
  let touchStartX = 0;

  el.infoBar.addEventListener('touchstart', (e) => {
    touchStartY = e.touches[0].clientY;
    touchStartX = e.touches[0].clientX;
  }, { passive: true });

  el.infoBar.addEventListener('touchend', (e) => {
    const dy = e.changedTouches[0].clientY - touchStartY;
    const dx = Math.abs(e.changedTouches[0].clientX - touchStartX);
    if (dy < -30 && dx < 60) openTripSheet();
  }, { passive: true });
}

// Long-press on trip value to open sheet
{
  let longPressTimer = null;

  function startLongPress() {
    longPressTimer = window.setTimeout(openTripSheet, 600);
  }
  function cancelLongPress() {
    window.clearTimeout(longPressTimer);
  }

  el.tripValue.addEventListener('mousedown', startLongPress);
  el.tripValue.addEventListener('touchstart', startLongPress, { passive: true });
  el.tripValue.addEventListener('mouseup', cancelLongPress);
  el.tripValue.addEventListener('mouseleave', cancelLongPress);
  el.tripValue.addEventListener('touchend', cancelLongPress);
  el.tripValue.addEventListener('touchcancel', cancelLongPress);
  el.tripValue.style.cursor = 'pointer';
  el.tripValue.style.userSelect = 'none';
}

// Swipe-down on the sheet to dismiss
{
  let tripSwipeStartY = 0;

  el.tripSheet.addEventListener('touchstart', (e) => {
    tripSwipeStartY = e.touches[0].clientY;
  }, { passive: true });

  el.tripSheet.addEventListener('touchend', (e) => {
    const dy = e.changedTouches[0].clientY - tripSwipeStartY;
    if (dy > 50 && el.tripSheet.scrollTop === 0) closeTripSheet();
  }, { passive: true });
}

// ── Init ──

el.wsUrl.value = currentSocketUrl;
updateSimulationUi();

// Auto-start simulation if served from preview (no gateway)
const params = new URLSearchParams(window.location.search);
if (params.has('sim') || params.get('simulation') === '1') {
  setSimulationMode(true);
} else {
  connectSocket();
}
