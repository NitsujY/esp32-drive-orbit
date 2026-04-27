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
  // New feature elements
  shiftBar: document.getElementById('shift-bar'),
  ambientCanvas: document.getElementById('ambient-canvas'),
  clockValue: document.getElementById('clock-value'),
  loadFill: document.getElementById('load-fill'),
  loadValue: document.getElementById('load-value'),
  boostFill: document.getElementById('boost-fill'),
  boostValue: document.getElementById('boost-value'),
  effValue: document.getElementById('eff-value'),
  gforceCanvas: document.getElementById('gforce-canvas'),
  gforceValue: document.getElementById('gforce-value'),
  // Theme toggle
  themeToggle: document.getElementById('theme-toggle'),
  // Weather bar
  weatherBar: document.getElementById('weather-bar'),
  weatherIcon: document.getElementById('weather-icon'),
  weatherTemp: document.getElementById('weather-temp'),
  weatherHumidity: document.getElementById('weather-humidity'),
  wifiStatus: document.getElementById('wifi-status'),
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

// ── Clock ──

function updateClock() {
  const now = new Date();
  const h = String(now.getHours()).padStart(2, '0');
  const m = String(now.getMinutes()).padStart(2, '0');
  el.clockValue.textContent = h + ':' + m;
}
updateClock();
setInterval(updateClock, 10000);

// ── Shift Lights ──

const shiftLeds = [...el.shiftBar.querySelectorAll('.shift-led')];
const SHIFT_LED_COUNT = shiftLeds.length;
const SHIFT_START_RPM = 1500; // LEDs start illuminating
const SHIFT_FLASH_RPM = 2200; // all flash at redline

function updateShiftLights(rpm) {
  if (rpm < SHIFT_START_RPM) {
    for (const led of shiftLeds) led.className = 'shift-led';
    return;
  }
  const frac = clamp((rpm - SHIFT_START_RPM) / (SHIFT_FLASH_RPM - SHIFT_START_RPM), 0, 1);
  const lit = Math.ceil(frac * SHIFT_LED_COUNT);
  const flash = rpm >= SHIFT_FLASH_RPM;

  for (let i = 0; i < SHIFT_LED_COUNT; i++) {
    if (i >= lit) {
      shiftLeds[i].className = 'shift-led';
    } else if (flash) {
      shiftLeds[i].className = 'shift-led on-red on-flash';
    } else {
      const zone = i / SHIFT_LED_COUNT;
      const cls = zone < 0.33 ? 'on-green' : zone < 0.66 ? 'on-yellow' : 'on-red';
      shiftLeds[i].className = 'shift-led ' + cls;
    }
  }
}

// ── Fuel Efficiency ──

const effSamples = [];
const EFF_WINDOW = 5;

function estimateFuelFlow(rpm) {
  // Simplified: idle ~0.8 L/h, scales with RPM
  return 0.8 + (rpm / 6000) * 8;
}

function computeL100(flowLph, speedKph) {
  if (speedKph < 3) return 0;
  return (flowLph / speedKph) * 100;
}

function updateFuelEfficiency(rpm, speed) {
  if (speed < 3) {
    el.effValue.textContent = '—';
    return;
  }
  const flow = estimateFuelFlow(rpm);
  const l100 = computeL100(flow, speed);
  effSamples.push(l100);
  if (effSamples.length > EFF_WINDOW) effSamples.shift();
  const avg = effSamples.reduce((a, b) => a + b, 0) / effSamples.length;
  el.effValue.textContent = avg.toFixed(1);
  // Color: efficient < 6 teal, moderate 6-10 amber, heavy >10 rose
  if (avg < 6) el.effValue.style.color = '#2dd4bf';
  else if (avg < 10) el.effValue.style.color = '#fbbf24';
  else el.effValue.style.color = '#fb7185';
}

// ── Engine Load & Boost ──

function updateEngineLoad(load) {
  const pct = clamp(load, 0, 100);
  el.loadValue.textContent = pct + '%';
  el.loadFill.style.width = pct + '%';
  if (pct < 50) el.loadFill.style.background = '#2dd4bf';
  else if (pct < 80) el.loadFill.style.background = '#fbbf24';
  else el.loadFill.style.background = '#fb7185';
}

function updateBoost(boostBar) {
  const clamped = clamp(boostBar, -1, 2);
  el.boostValue.textContent = clamped.toFixed(1);
  // Fill: map -1..2 bar range, typical 0-1.5 is normal turbo
  const fillPct = clamp(((clamped + 0.2) / 1.8) * 100, 0, 100);
  el.boostFill.style.width = fillPct + '%';
  if (clamped > 1.4) {
    el.boostFill.style.background = '#fb7185';
    el.boostValue.style.color = '#fb7185';
  } else if (clamped > 0.8) {
    el.boostFill.style.background = '#fbbf24';
    el.boostValue.style.color = '#fbbf24';
  } else {
    el.boostFill.style.background = '#2dd4bf';
    el.boostValue.style.color = '';
  }
}

// ── G-Force Crosshair ──

const gforceCtx = el.gforceCanvas.getContext('2d');

function drawGForce(lat, lon) {
  const w = el.gforceCanvas.width;
  const h = el.gforceCanvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const maxG = 1.5;
  const r = (w / 2) - 2;

  gforceCtx.clearRect(0, 0, w, h);

  // Circle outline
  gforceCtx.beginPath();
  gforceCtx.arc(cx, cy, r, 0, Math.PI * 2);
  gforceCtx.strokeStyle = 'rgba(255,255,255,0.1)';
  gforceCtx.lineWidth = 1;
  gforceCtx.stroke();

  // Crosshair lines
  gforceCtx.beginPath();
  gforceCtx.moveTo(cx, cy - r);
  gforceCtx.lineTo(cx, cy + r);
  gforceCtx.moveTo(cx - r, cy);
  gforceCtx.lineTo(cx + r, cy);
  gforceCtx.strokeStyle = 'rgba(255,255,255,0.06)';
  gforceCtx.stroke();

  // G-force dot
  const dx = clamp(lat / maxG, -1, 1) * r;
  const dy = clamp(-lon / maxG, -1, 1) * r;
  const totalG = Math.sqrt(lat * lat + lon * lon);

  let dotColor = '#2dd4bf';
  if (totalG > 0.8) dotColor = '#fb7185';
  else if (totalG > 0.4) dotColor = '#fbbf24';

  gforceCtx.beginPath();
  gforceCtx.arc(cx + dx, cy + dy, 2.5, 0, Math.PI * 2);
  gforceCtx.fillStyle = dotColor;
  gforceCtx.fill();

  // Total G readout
  el.gforceValue.textContent = totalG.toFixed(1);
}

drawGForce(0, 0);

// ── Ambient Animation (Weather + Idle) ──

const ambientCtx = el.ambientCanvas.getContext('2d');
let ambientParticles = [];
let ambientAnimFrame = null;
let ambientMode = 'idle'; // 'idle' | 'rain' | 'snow' | 'clear'
let idleTimer = 0;
const IDLE_TRIGGER_MS = 3000;

function resizeAmbientCanvas() {
  const rect = el.ambientCanvas.parentElement.getBoundingClientRect();
  el.ambientCanvas.width = rect.width * (window.devicePixelRatio || 1);
  el.ambientCanvas.height = rect.height * (window.devicePixelRatio || 1);
  ambientCtx.scale(window.devicePixelRatio || 1, window.devicePixelRatio || 1);
}
resizeAmbientCanvas();
window.addEventListener('resize', resizeAmbientCanvas);

function spawnIdleParticle() {
  const rect = el.ambientCanvas.parentElement.getBoundingClientRect();
  return {
    x: Math.random() * rect.width,
    y: Math.random() * rect.height,
    r: 1 + Math.random() * 2,
    phase: Math.random() * Math.PI * 2,
    speed: 0.3 + Math.random() * 0.4,
    drift: (Math.random() - 0.5) * 0.3,
  };
}

function spawnRainParticle() {
  const rect = el.ambientCanvas.parentElement.getBoundingClientRect();
  return {
    x: Math.random() * rect.width,
    y: -5,
    len: 6 + Math.random() * 10,
    speed: 3 + Math.random() * 4,
    alpha: 0.15 + Math.random() * 0.2,
  };
}

function spawnSnowParticle() {
  const rect = el.ambientCanvas.parentElement.getBoundingClientRect();
  return {
    x: Math.random() * rect.width,
    y: -3,
    r: 1 + Math.random() * 2,
    speed: 0.5 + Math.random() * 1,
    drift: (Math.random() - 0.5) * 0.5,
    alpha: 0.2 + Math.random() * 0.2,
  };
}

function tickAmbient() {
  const rect = el.ambientCanvas.parentElement.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;
  ambientCtx.clearRect(0, 0, w, h);

  if (ambientMode === 'idle') {
    // Breathing particles
    while (ambientParticles.length < 20) ambientParticles.push(spawnIdleParticle());
    const t = performance.now() / 1000;
    for (const p of ambientParticles) {
      const breath = 0.3 + 0.7 * ((Math.sin(t * p.speed + p.phase) + 1) / 2);
      ambientCtx.beginPath();
      ambientCtx.arc(p.x + Math.sin(t * 0.5 + p.phase) * 8, p.y, p.r, 0, Math.PI * 2);
      ambientCtx.fillStyle = `rgba(45,212,191,${0.15 * breath})`;
      ambientCtx.fill();
    }
  } else if (ambientMode === 'rain') {
    if (Math.random() < 0.4) ambientParticles.push(spawnRainParticle());
    for (let i = ambientParticles.length - 1; i >= 0; i--) {
      const p = ambientParticles[i];
      p.y += p.speed;
      ambientCtx.beginPath();
      ambientCtx.moveTo(p.x, p.y);
      ambientCtx.lineTo(p.x - 0.5, p.y - p.len);
      ambientCtx.strokeStyle = `rgba(147,197,253,${p.alpha})`;
      ambientCtx.lineWidth = 1;
      ambientCtx.stroke();
      if (p.y > h + 10) ambientParticles.splice(i, 1);
    }
  } else if (ambientMode === 'snow') {
    if (Math.random() < 0.15) ambientParticles.push(spawnSnowParticle());
    for (let i = ambientParticles.length - 1; i >= 0; i--) {
      const p = ambientParticles[i];
      p.y += p.speed;
      p.x += p.drift + Math.sin(p.y * 0.02) * 0.3;
      ambientCtx.beginPath();
      ambientCtx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
      ambientCtx.fillStyle = `rgba(255,255,255,${p.alpha})`;
      ambientCtx.fill();
      if (p.y > h + 5) ambientParticles.splice(i, 1);
    }
  }
  // 'clear' = no particles

  ambientAnimFrame = requestAnimationFrame(tickAmbient);
}

function setAmbientMode(mode) {
  if (mode === ambientMode) return;
  ambientMode = mode;
  ambientParticles = [];
}

// Determine weather mode from WMO weather code
function weatherCodeToMode(wc) {
  if (wc == null || wc === 0 || wc === 1) return 'clear';
  if (wc >= 51 && wc <= 67) return 'rain';  // drizzle/rain
  if (wc >= 71 && wc <= 77) return 'snow';  // snow
  if (wc >= 80 && wc <= 82) return 'rain';  // showers
  if (wc >= 85 && wc <= 86) return 'snow';  // snow showers
  if (wc >= 95) return 'rain';              // thunderstorm
  return 'clear';
}

// Start ambient animation loop
tickAmbient();

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
    const rpmVal = rpmHistory[i];
    if (rpmVal < 1500) bar.style.background = '#2dd4bf';
    else if (rpmVal < 2000) bar.style.background = '#fbbf24';
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

  // ─── New Features ───

  // Shift lights
  updateShiftLights(rpm);

  // Engine load
  const engineLoad = data.el ?? simulateEngineLoad(rpm, speed);
  updateEngineLoad(engineLoad);

  // Turbo boost
  const boostBar = data.boost ?? simulateBoost(rpm, speed);
  updateBoost(boostBar);

  // Fuel efficiency
  updateFuelEfficiency(rpm, speed);

  // G-force
  const latG = (data.lat_g ?? simulateLatG(speed)) ;
  const lonG = (data.lon_g ?? simulateLonG(data.acc)) ;
  drawGForce(latG, lonG);

  // Ambient weather/idle
  if (speed === 0) {
    idleTimer += 100;
    if (idleTimer >= IDLE_TRIGGER_MS) setAmbientMode('idle');
  } else {
    idleTimer = 0;
    const wc = data.wc ?? 0;
    setAmbientMode(weatherCodeToMode(wc));
  }

  // Weather bar
  updateWeatherBar(data);
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

// ── Simulation Helpers for New Gauges ──

function simulateEngineLoad(rpm, speed) {
  // Rough estimate: idle ~20%, scales with RPM and speed
  const rpmFactor = clamp(rpm / MAX_RPM, 0, 1);
  const spdFactor = clamp(speed / 120, 0, 1);
  return Math.round(15 + rpmFactor * 55 + spdFactor * 20 + Math.sin(performance.now() / 800) * 3);
}

function simulateBoost(rpm, speed) {
  // No boost at low RPM, builds with RPM
  if (rpm < 1800) return -0.1 + Math.random() * 0.05;
  const rpmFrac = clamp((rpm - 1800) / 4200, 0, 1);
  return rpmFrac * 1.2 + Math.sin(performance.now() / 600) * 0.05;
}

function simulateLatG(speed) {
  if (speed < 5) return 0;
  // Simulate gentle cornering from steering
  return Math.sin(performance.now() / 2400) * clamp(speed / 100, 0, 1) * 0.3;
}

function simulateLonG(acc) {
  // Use acceleration data if available, otherwise simulate
  if (acc != null) return clamp(acc / 1000, -1.5, 1.5);
  return Math.sin(performance.now() / 1800) * 0.15;
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
    wt: 22 + Math.round(Math.sin(simulationPhase * 0.01) * 4), // ~18–26°C
    wc: [0, 1, 2, 3, 51, 61][Math.floor(simulationPhase / 10) % 6],
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

// Tap the trip odometer chip to open the sheet.
// Swipe-up from the info bar is intentionally avoided — on iPhone it conflicts
// with the system home-indicator gesture at the bottom of the screen.
el.tripValue.addEventListener('click', openTripSheet);

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

// Brief pulse on the trip chip to hint it's tappable
window.setTimeout(() => {
  el.tripValue.classList.add('tap-hint');
  el.tripValue.addEventListener('animationend', () => el.tripValue.classList.remove('tap-hint'), { once: true });
}, 1200);

// Auto-start simulation if served from preview (no gateway)
const params = new URLSearchParams(window.location.search);
if (params.has('sim') || params.get('simulation') === '1') {
  setSimulationMode(true);
} else {
  connectSocket();
}

// ── Theme Toggle ──
// Two modes, both dark:
//   dark  (default) — deep navy tint, data-theme unset / "dark"
//   black           — pure black, data-theme="black"

let isBlackTheme = false;

function applyTheme(black) {
  isBlackTheme = black;
  document.documentElement.setAttribute('data-theme', black ? 'black' : 'dark');
  document.querySelector('meta[name="theme-color"]').setAttribute(
    'content', black ? '#000000' : '#0c0c0c'
  );
  try { localStorage.setItem('driveOrbitTheme', black ? 'black' : 'dark'); } catch (_) {}
}

// Restore saved theme preference
{
  const saved = (() => { try { return localStorage.getItem('driveOrbitTheme'); } catch (_) { return null; } })();
  applyTheme(saved === 'black');
}

el.themeToggle.addEventListener('click', () => applyTheme(!isBlackTheme));

// ── Weather Bar ──

const WMO_ICONS = {
  0: '☀️', 1: '🌤', 2: '⛅', 3: '☁️',
  45: '🌫', 48: '🌫',
  51: '🌦', 53: '🌦', 55: '🌧',
  61: '🌧', 63: '🌧', 65: '🌧',
  71: '🌨', 73: '🌨', 75: '❄️',
  80: '🌦', 81: '🌧', 82: '⛈',
  95: '⛈', 96: '⛈', 99: '⛈',
};

function wmoIcon(code) {
  if (code == null) return '☁';
  return WMO_ICONS[code] ?? '🌡';
}

function updateWeatherBar(data) {
  const wifiOn = data.wifi === 1 || data.wifi === true;
  const tempC = data.wt ?? data.weather_temp_c;
  const wc = data.wc ?? data.weather_code;

  el.weatherBar.classList.toggle('wifi-off', !wifiOn);
  el.wifiStatus.textContent = wifiOn ? 'online' : 'offline';

  if (wifiOn && tempC != null && tempC !== -128) {
    el.weatherTemp.textContent = tempC + '°C';
    el.weatherIcon.textContent = wmoIcon(wc);
    // Simulate humidity from weather code (real data would come from API)
    const simHumidity = wc >= 51 ? 75 + Math.round((wc - 51) / 2) : 40 + Math.round(Math.random() * 10);
    el.weatherHumidity.textContent = Math.min(simHumidity, 99) + '%';
  } else if (!wifiOn) {
    el.weatherTemp.textContent = '--°';
    el.weatherHumidity.textContent = '--%';
    el.weatherIcon.textContent = '☁';
  }
}
