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
  themeToggle: document.getElementById('theme-toggle'),
  wifiStatus: document.getElementById('wifi-status'),
  weatherIcon: document.getElementById('weather-icon'),
  weatherTemp: document.getElementById('weather-temp'),
  weatherCond: document.getElementById('weather-cond'),
  weatherNote: document.getElementById('weather-note'),
  settingsToggle: document.getElementById('settings-toggle'),
  settingsPanel: document.getElementById('settings-panel'),
  settingsBackdrop: document.getElementById('settings-backdrop'),
  settingsClose: document.getElementById('settings-close'),
  wsUrl: document.getElementById('ws-url'),
  resetButton: document.getElementById('reset-button'),
  simulationButton: document.getElementById('simulation-button'),
  simulationStatus: document.getElementById('simulation-status'),
  fuelCluster: document.querySelector('.fuel-cluster'),
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
};

const WAVEFORM_BARS = 60;
const MAX_RPM = 6000;
const THEME_KEY = 'drive-orbit-theme';
const WEATHER_LOOKUP = {
  0: { icon: '☀', label: 'clear' },
  1: { icon: '🌤', label: 'mainly clear' },
  2: { icon: '⛅', label: 'partly cloudy' },
  3: { icon: '☁', label: 'cloudy' },
  45: { icon: '🌫', label: 'fog' },
  48: { icon: '🌫', label: 'fog' },
  51: { icon: '🌦', label: 'drizzle' },
  53: { icon: '🌦', label: 'drizzle' },
  55: { icon: '🌧', label: 'rain' },
  61: { icon: '🌧', label: 'rain' },
  63: { icon: '🌧', label: 'rain' },
  65: { icon: '🌧', label: 'rain' },
  71: { icon: '❄', label: 'snow' },
  73: { icon: '❄', label: 'snow' },
  75: { icon: '❄', label: 'snow' },
  80: { icon: '🌧', label: 'showers' },
  81: { icon: '🌧', label: 'showers' },
  82: { icon: '⛈', label: 'storm' },
  95: { icon: '⛈', label: 'storm' },
};

const rpmHistory = [];
const waveformBars = [];

for (let index = 0; index < WAVEFORM_BARS; index += 1) {
  const bar = document.createElement('div');
  bar.className = 'bar';
  bar.style.height = '2px';
  el.waveform.appendChild(bar);
  waveformBars.push(bar);
}

let socket = null;
let reconnectTimer = null;
let reconnectAttempt = 0;
let settingsOpen = false;
let simulationMode = false;
let simulationTimer = null;
let simulationSeq = 0;
let simulationPhase = 0;
let simulationUptimeMs = 0;
let lastTickAt = performance.now();
let tripStartTime = Date.now();
let tripDistanceKm = 0;
let tripSpeedSum = 0;
let tripSpeedCount = 0;
let tripMaxSpeed = 0;
let tripHarshAccelCount = 0;
let tripHarshBrakeCount = 0;
let tripOverspeedCount = 0;
let lastHarshAccelAt = 0;
let lastHarshBrakeAt = 0;
let lastOverspeedAt = 0;
let tripSheetOpen = false;
let ambientMode = 'clear';
let ambientParticles = [];
let currentSocketUrl = resolveDefaultSocketUrl();

const speedBuffer = [];
const shiftLeds = [...el.shiftBar.querySelectorAll('.shift-led')];
const shiftCount = shiftLeds.length;

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function isNativeShell() {
  return typeof window.Capacitor?.isNativePlatform === 'function' && window.Capacitor.isNativePlatform();
}

function updateClock() {
  const now = new Date();
  el.clockValue.textContent = String(now.getHours()).padStart(2, '0') + ':' + String(now.getMinutes()).padStart(2, '0');
}

function setTheme(theme) {
  const nextTheme = theme === 'black' ? 'black' : 'dark';
  document.body.dataset.theme = nextTheme;
  localStorage.setItem(THEME_KEY, nextTheme);
  if (el.themeToggle) {
    el.themeToggle.textContent = nextTheme === 'black' ? 'DARK' : 'BLACK';
    el.themeToggle.setAttribute('aria-label', nextTheme === 'black' ? 'Switch to dark theme' : 'Switch to black theme');
  }
}

function updateWifiIndicator(connected) {
  if (!el.wifiStatus) {
    return;
  }

  el.wifiStatus.className = 'wifi-status ' + (connected ? 'live' : 'error');
  el.wifiStatus.title = connected ? 'Wi-Fi linked' : 'Wi-Fi offline';
  el.wifiStatus.setAttribute('aria-label', connected ? 'Wi-Fi linked' : 'Wi-Fi offline');
}

function updateWeatherUi(snapshot) {
  if (!el.weatherIcon || !el.weatherTemp || !el.weatherCond) {
    return;
  }

  const meta = WEATHER_LOOKUP[snapshot.wc] || { icon: '☁', label: 'weather' };
  el.weatherIcon.textContent = meta.icon;
  el.weatherTemp.textContent = snapshot.wt === undefined || snapshot.wt === null ? '--°C' : `${Math.round(snapshot.wt)}°C`;
  el.weatherCond.textContent = meta.label;
  if (el.weatherNote) {
    el.weatherNote.textContent = snapshot.wifi ? 'Wi-Fi • open-meteo' : 'Wi-Fi offline';
  }
}

function setStatusDot(kind, title) {
  el.statusDot.className = 'status-dot' + (kind ? ` ${kind}` : '');
  el.statusDot.title = title;
}

function updateShiftLights(rpm) {
  const shiftStart = 1500;
  const shiftFlash = 2200;

  if (rpm < shiftStart) {
    for (const led of shiftLeds) {
      led.className = 'shift-led';
    }
    return;
  }

  const progress = clamp((rpm - shiftStart) / (shiftFlash - shiftStart), 0, 1);
  const litCount = Math.ceil(progress * shiftCount);
  const flashing = rpm >= shiftFlash;

  for (let index = 0; index < shiftCount; index += 1) {
    if (index >= litCount) {
      shiftLeds[index].className = 'shift-led';
      continue;
    }

    if (flashing) {
      shiftLeds[index].className = 'shift-led on-red on-flash';
      continue;
    }

    const phase = index / shiftCount;
    const tone = phase < 0.33 ? 'on-green' : phase < 0.66 ? 'on-yellow' : 'on-red';
    shiftLeds[index].className = `shift-led ${tone}`;
  }
}

function updateEngineLoad(value) {
  const load = clamp(value, 0, 100);
  el.loadValue.textContent = `${load}%`;
  el.loadFill.style.width = `${load}%`;
  el.loadFill.style.background = load < 50 ? '#2dd4bf' : load < 80 ? '#fbbf24' : '#fb7185';
}

function updateBoost(value) {
  const boost = clamp(value, -1, 2);
  el.boostValue.textContent = boost.toFixed(1);
  const fill = clamp(((boost + 0.2) / 1.8) * 100, 0, 100);
  el.boostFill.style.width = `${fill}%`;
  if (boost > 1.4) {
    el.boostFill.style.background = '#fb7185';
    el.boostValue.style.color = '#fb7185';
  } else if (boost > 0.8) {
    el.boostFill.style.background = '#fbbf24';
    el.boostValue.style.color = '#fbbf24';
  } else {
    el.boostFill.style.background = '#2dd4bf';
    el.boostValue.style.color = '';
  }
}

function updateFuelEfficiency(rpm, speed) {
  if (speed < 3) {
    el.effValue.textContent = '—';
    return;
  }

  const flow = 0.8 + (rpm / MAX_RPM) * 8;
  const l100 = (flow / speed) * 100;
  rpmHistory.push(l100);
  if (rpmHistory.length > 5) {
    rpmHistory.shift();
  }

  const avg = rpmHistory.reduce((sum, sample) => sum + sample, 0) / rpmHistory.length;
  el.effValue.textContent = avg.toFixed(1);
  el.effValue.style.color = avg < 6 ? '#2dd4bf' : avg < 10 ? '#fbbf24' : '#fb7185';
}

function drawGForce(latG, lonG) {
  const ctx = el.gforceCanvas.getContext('2d');
  const width = el.gforceCanvas.width;
  const height = el.gforceCanvas.height;
  const centerX = width / 2;
  const centerY = height / 2;
  const radius = width / 2 - 2;
  const gFactor = 1.5;

  ctx.clearRect(0, 0, width, height);
  ctx.beginPath();
  ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
  ctx.strokeStyle = 'rgba(255,255,255,0.1)';
  ctx.lineWidth = 1;
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(centerX, centerY - radius);
  ctx.lineTo(centerX, centerY + radius);
  ctx.moveTo(centerX - radius, centerY);
  ctx.lineTo(centerX + radius, centerY);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.stroke();

  const x = clamp(latG / gFactor, -1, 1) * radius;
  const y = clamp(-lonG / gFactor, -1, 1) * radius;
  const magnitude = Math.sqrt(latG * latG + lonG * lonG);
  const color = magnitude > 0.8 ? '#fb7185' : magnitude > 0.4 ? '#fbbf24' : '#2dd4bf';

  ctx.beginPath();
  ctx.arc(centerX + x, centerY + y, 2.5, 0, Math.PI * 2);
  ctx.fillStyle = color;
  ctx.fill();
  el.gforceValue.textContent = magnitude.toFixed(1);
}

function resolveDefaultSocketUrl() {
  return isNativeShell() ? 'ws://carconsole.local/ws' : `ws://${window.location.host}/ws`;
}

function resolveInitialSocketUrl() {
  const manual = new URLSearchParams(window.location.search).get('ws');
  if (manual) {
    localStorage.setItem('driveOrbitWsUrl', manual);
    return manual;
  }

  return localStorage.getItem('driveOrbitWsUrl') || resolveDefaultSocketUrl();
}

function estimateGear(speed) {
  if (speed < 2) return 'P';
  if (speed < 12) return 'D1';
  if (speed < 28) return 'D2';
  if (speed < 44) return 'D3';
  if (speed < 62) return 'D4';
  if (speed < 82) return 'D5';
  return 'D6';
}

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

function updateTripMetrics(speed, nowMs) {
  if (speed > 0) {
    tripSpeedSum += speed;
    tripSpeedCount += 1;
    tripMaxSpeed = Math.max(tripMaxSpeed, speed);
  }

  if (speed > 120 && nowMs - lastOverspeedAt > 2500) {
    tripOverspeedCount += 1;
    lastOverspeedAt = nowMs;
  }

  speedBuffer.push(speed);
  if (speedBuffer.length > 5) {
    speedBuffer.shift();
  }

  if (speedBuffer.length === 5) {
    const delta = speedBuffer[4] - speedBuffer[0];
    if (delta > 18 && nowMs - lastHarshAccelAt > 2500) {
      tripHarshAccelCount += 1;
      lastHarshAccelAt = nowMs;
    } else if (delta < -18 && nowMs - lastHarshBrakeAt > 2500) {
      tripHarshBrakeCount += 1;
      lastHarshBrakeAt = nowMs;
    }
  }
}

function computeTripScore() {
  let score = 100;
  score -= tripHarshAccelCount * 3;
  score -= tripHarshBrakeCount * 4;
  score -= tripOverspeedCount * 5;
  score = clamp(score, 0, 100);

  let grade = 'S';
  let label = 'Silky Smooth';
  if (score < 90) { grade = 'A'; label = 'Solid Driver'; }
  if (score < 75) { grade = 'B'; label = 'Room to Improve'; }
  if (score < 60) { grade = 'C'; label = 'Rough Edges'; }
  if (score < 40) { grade = 'D'; label = 'Take it Easy'; }

  const smoothness = clamp(100 - tripHarshAccelCount * 14 - tripHarshBrakeCount * 18, 0, 100);
  const consistency = tripSpeedCount > 0 ? clamp(Math.round(score * 0.6 + smoothness * 0.4), 0, 100) : 0;
  return { score, grade, label, smoothness, consistency };
}

function formatDuration(ms) {
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return hours > 0 ? `${hours}:${String(minutes).padStart(2, '0')}` : `${minutes}:${String(seconds).padStart(2, '0')}`;
}

function gradeClass(grade) {
  if (grade === 'B') return 'grade-b';
  if (grade === 'C' || grade === 'D') return 'grade-cd';
  return '';
}

function populateTripSheet() {
  const elapsed = Date.now() - tripStartTime;
  const avgSpeed = tripSpeedCount > 0 ? Math.round(tripSpeedSum / tripSpeedCount) : 0;
  const result = computeTripScore();
  const tone = gradeClass(result.grade);

  el.tsDuration.textContent = formatDuration(elapsed);
  el.tsDistance.textContent = `${tripDistanceKm.toFixed(1)} km`;
  el.tsAvgSpeed.textContent = `${avgSpeed} km/h`;
  el.tsMaxSpeed.textContent = `${tripMaxSpeed} km/h`;
  el.tsScoreNum.textContent = result.score > 0 ? String(result.score) : '—';
  el.tsScoreNum.className = 'trip-score-badge' + (tone ? ` ${tone}` : '');
  el.tsScoreGrade.textContent = tripSpeedCount > 0 ? `/ ${result.grade}` : '';
  el.tsScoreGrade.className = 'trip-score-grade' + (tone ? ` ${tone}` : '');
  el.tsScoreLabel.textContent = tripSpeedCount > 0 ? result.label : 'No trip data yet';
  el.tsBarSmoothness.style.width = `${result.smoothness}%`;
  el.tsBarSmoothness.className = 'trip-bar-fill' + (tone ? ` ${tone}` : '');
  el.tsBarConsistency.style.width = `${result.consistency}%`;
  el.tsBarConsistency.className = 'trip-bar-fill' + (tone ? ` ${tone}` : '');
  el.tsEvAccel.textContent = `${tripHarshAccelCount} harsh acceleration${tripHarshAccelCount === 1 ? '' : 's'}`;
  el.tsEvBrake.textContent = `${tripHarshBrakeCount} harsh braking`;
  el.tsEvOverspeed.textContent = `${tripOverspeedCount} over-speed event${tripOverspeedCount === 1 ? '' : 's'}`;
}

function openTripSheet() {
  if (tripSheetOpen) return;
  populateTripSheet();
  tripSheetOpen = true;
  el.tripSheet.hidden = false;
  el.tripSheetBackdrop.hidden = false;
  el.tripSheet.classList.add('is-open');
  el.tripSheetBackdrop.classList.add('is-open');
}

function closeTripSheet() {
  if (!tripSheetOpen) return;
  tripSheetOpen = false;
  el.tripSheet.classList.remove('is-open');
  el.tripSheetBackdrop.classList.remove('is-open');
  window.setTimeout(() => {
    if (!tripSheetOpen) {
      el.tripSheet.hidden = true;
      el.tripSheetBackdrop.hidden = true;
    }
  }, 320);
}

function setSettingsOpen(open) {
  settingsOpen = open;
  el.settingsPanel.hidden = !open;
  el.settingsBackdrop.hidden = !open;
  el.settingsPanel.classList.toggle('is-open', open);
  el.settingsBackdrop.classList.toggle('is-open', open);
  el.settingsToggle.setAttribute('aria-expanded', String(open));
}

function weatherMode(code) {
  if (code === 61 || code === 63 || code === 65 || code === 80 || code === 81) return 'rain';
  if (code === 71 || code === 73 || code === 75 || code === 85 || code === 86) return 'snow';
  if (code === 45 || code === 48) return 'fog';
  return 'clear';
}

function setAmbientMode(nextMode) {
  if (nextMode === ambientMode) return;
  ambientMode = nextMode;
  ambientParticles = [];
}

function spawnAmbientParticle() {
  const box = el.ambientCanvas.parentElement.getBoundingClientRect();
  return {
    x: Math.random() * box.width,
    y: Math.random() * box.height,
    r: 1 + Math.random() * 2,
    speed: 0.3 + Math.random() * 0.4,
    drift: (Math.random() - 0.5) * 0.3,
    phase: Math.random() * Math.PI * 2,
    alpha: 0.12 + Math.random() * 0.2,
    len: 6 + Math.random() * 10,
  };
}

function drawAmbient() {
  const ctx = el.ambientCanvas.getContext('2d');
  const box = el.ambientCanvas.parentElement.getBoundingClientRect();
  const width = box.width;
  const height = box.height;

  ctx.clearRect(0, 0, width, height);

  if (ambientMode === 'clear') {
    while (ambientParticles.length < 18) ambientParticles.push(spawnAmbientParticle());
    const time = performance.now() / 1000;
    for (const particle of ambientParticles) {
      const pulse = 0.4 + 0.6 * ((Math.sin(time * particle.speed + particle.phase) + 1) / 2);
      ctx.beginPath();
      ctx.arc(particle.x + Math.sin(time * 0.5 + particle.phase) * 8, particle.y, particle.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(125,249,198,${0.1 * pulse})`;
      ctx.fill();
    }
  } else if (ambientMode === 'rain') {
    if (Math.random() < 0.35) ambientParticles.push(spawnAmbientParticle());
    for (let index = ambientParticles.length - 1; index >= 0; index -= 1) {
      const particle = ambientParticles[index];
      particle.y += particle.len * 0.45;
      ctx.beginPath();
      ctx.moveTo(particle.x, particle.y);
      ctx.lineTo(particle.x - 0.5, particle.y - particle.len);
      ctx.strokeStyle = `rgba(147,197,253,${particle.alpha})`;
      ctx.lineWidth = 1;
      ctx.stroke();
      if (particle.y > height + 10) ambientParticles.splice(index, 1);
    }
  } else if (ambientMode === 'snow') {
    if (Math.random() < 0.15) ambientParticles.push(spawnAmbientParticle());
    for (let index = ambientParticles.length - 1; index >= 0; index -= 1) {
      const particle = ambientParticles[index];
      particle.y += particle.speed;
      particle.x += particle.drift + Math.sin(particle.y * 0.02) * 0.3;
      ctx.beginPath();
      ctx.arc(particle.x, particle.y, particle.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(255,255,255,${particle.alpha})`;
      ctx.fill();
      if (particle.y > height + 5) ambientParticles.splice(index, 1);
    }
  } else if (ambientMode === 'fog') {
    ctx.fillStyle = 'rgba(255,255,255,0.03)';
    ctx.fillRect(0, 0, width, height);
  }

  requestAnimationFrame(drawAmbient);
}

function connectSocket() {
  if (simulationMode) return;
  window.clearTimeout(reconnectTimer);
  currentSocketUrl = el.wsUrl.value.trim() || resolveInitialSocketUrl();
  el.wsUrl.value = currentSocketUrl;
  localStorage.setItem('driveOrbitWsUrl', currentSocketUrl);
  setStatusDot('pending', 'Connecting');

  const nextSocket = new WebSocket(currentSocketUrl);
  socket = nextSocket;

  nextSocket.addEventListener('open', () => {
    if (socket !== nextSocket) return;
    reconnectAttempt = 0;
    window.clearTimeout(reconnectTimer);
    setStatusDot('live', 'Connected');
    setSettingsOpen(false);
  });

  nextSocket.addEventListener('message', event => {
    if (socket !== nextSocket) return;
    applyTelemetry(JSON.parse(event.data));
  });

  nextSocket.addEventListener('close', () => {
    if (socket !== nextSocket) return;
    socket = null;
    if (!simulationMode) {
      setStatusDot('error', 'Disconnected');
      scheduleReconnect();
    }
  });

  nextSocket.addEventListener('error', () => {
    if (socket === nextSocket) {
      setStatusDot('error', 'Connection error');
    }
  });
}

function scheduleReconnect() {
  if (simulationMode || (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING))) {
    return;
  }

  window.clearTimeout(reconnectTimer);
  reconnectAttempt += 1;
  const delay = clamp(500 * 2 ** (reconnectAttempt - 1), 500, 8000);
  reconnectTimer = window.setTimeout(connectSocket, delay);
}

function simulateEngineLoad(rpm, speed) {
  const rpmRatio = clamp(rpm / MAX_RPM, 0, 1);
  const speedRatio = clamp(speed / 120, 0, 1);
  return Math.round(15 + rpmRatio * 55 + speedRatio * 20 + Math.sin(performance.now() / 800) * 3);
}

function simulateBoost(rpm, speed) {
  if (rpm < 1800) {
    return -0.1 + Math.random() * 0.05;
  }

  return clamp((rpm - 1800) / 4200, 0, 1) * 1.2 + Math.sin(performance.now() / 600) * 0.05;
}

function simulateLatG(speed) {
  if (speed < 5) return 0;
  return Math.sin(performance.now() / 2400) * clamp(speed / 100, 0, 1) * 0.3;
}

function simulateLonG(acceleration) {
  return acceleration != null ? clamp(acceleration / 1000, -1.5, 1.5) : Math.sin(performance.now() / 1800) * 0.15;
}

function applyTelemetry(snapshot) {
  const speed = snapshot.spd ?? 0;
  const rpm = snapshot.rpm ?? 0;
  const fuel = snapshot.fuel ?? 0;
  const range = snapshot.rng ?? 0;
  const nowMs = Date.now();

  el.speedValue.textContent = speed;
  rpmHistory.push(rpm);
  if (rpmHistory.length > WAVEFORM_BARS) {
    rpmHistory.shift();
  }

  for (let index = 0; index < WAVEFORM_BARS; index += 1) {
    const sample = rpmHistory[index] || 0;
    const height = 2 + clamp(sample / MAX_RPM, 0, 1) * 30;
    const bar = waveformBars[index];
    bar.style.height = `${height}px`;
    bar.style.background = sample < 1500 ? '#2dd4bf' : sample < 2000 ? '#fbbf24' : '#fb7185';
  }

  el.rpmValue.textContent = rpm.toLocaleString();
  el.gearValue.textContent = estimateGear(speed);
  el.fuelValue.textContent = `${fuel}%`;
  el.fuelFill.style.width = `${clamp(fuel, 0, 100)}%`;
  el.fuelCluster.classList.toggle('low', fuel > 0 && fuel <= 20);
  el.fuelCluster.classList.toggle('critical', fuel > 0 && fuel <= 10);
  el.rangeValue.textContent = `${range} km`;

  tripDistanceKm += speed / 36000;
  el.tripValue.textContent = `${tripDistanceKm.toFixed(1)} km`;
  updateTripMetrics(speed, nowMs);

  const weatherCode = snapshot.wc ?? 0;
  updateWeatherUi(snapshot);
  updateWifiIndicator(Boolean(snapshot.wifi ?? snapshot.wifi_connected ?? 0));
  setAmbientMode(weatherMode(weatherCode));

  updateShiftLights(rpm);
  updateEngineLoad(snapshot.el ?? simulateEngineLoad(rpm, speed));
  updateBoost(snapshot.boost ?? simulateBoost(rpm, speed));
  updateFuelEfficiency(rpm, speed);
  drawGForce(snapshot.lat_g ?? simulateLatG(speed), snapshot.lon_g ?? simulateLonG(snapshot.acc));

  if (tripSheetOpen) {
    populateTripSheet();
  }

  if (speed === 0 && snapshot.fresh) {
    setStatusDot('live', 'Idle');
  }
}

function simulateSnapshot() {
  simulationSeq += 1;
  simulationPhase += 0.12;
  simulationUptimeMs += 100;

  const phase = simulationPhase % 56;
  let speed = 0;
  if (phase < 3) speed = 0;
  else if (phase < 8) speed = ((phase - 3) / 5) * 26;
  else if (phase < 14) speed = 26 + Math.sin((phase - 8) * 0.72) * 1.4;
  else if (phase < 20) speed = 26 + ((phase - 14) / 6) * 44;
  else if (phase < 25) speed = 70 + Math.sin((phase - 20) * 0.64) * 1.6;
  else if (phase < 31) speed = 70 - ((phase - 25) / 6) * 58;
  else if (phase < 35) speed = 12 - ((phase - 31) / 4) * 12;
  else if (phase < 39) speed = 0;
  else if (phase < 44) speed = ((phase - 39) / 5) * 24;
  else if (phase < 48) speed = 24 + Math.sin((phase - 44) * 0.9) * 1.1;
  else if (phase < 54) speed = 24 - ((phase - 48) / 6) * 24;

  const rpm = clamp(Math.round(780 + speed * 17 + Math.max(speed - 40, 0) * 1.1 + Math.sin(simulationPhase * 1.4) * 54), 760, 3400);
  const fuel = clamp(Math.round(82 - (simulationSeq / 1800) % 58), 24, 82);
  const range = Math.round(fuel * 4.6 * clamp(1.02 - speed / 260, 0.72, 1.02));
  const weatherCycle = [0, 2, 61, 71][simulationSeq % 4];
  const temp = 18 + Math.round(Math.sin(simulationPhase * 0.8) * 5);

  return {
    v: 1,
    seq: simulationSeq,
    spd: Math.round(speed),
    rpm,
    fuel,
    rng: range,
    wt: temp,
    wc: weatherCycle,
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
    applyTelemetry(simulateSnapshot(dt));
  }, 100);
}

function stopSimulation() {
  if (simulationTimer) {
    window.clearInterval(simulationTimer);
    simulationTimer = null;
  }
}

function updateSimulationUi() {
  el.simulationButton.textContent = simulationMode ? 'Simulation On' : 'Simulation Off';
  el.simulationStatus.textContent = simulationMode ? 'Using browser demo telemetry.' : 'Use live gateway telemetry.';
  el.resetButton.disabled = simulationMode;
}

function setSimulationMode(enabled) {
  simulationMode = enabled;
  updateSimulationUi();
  if (enabled) {
    if (socket) {
      socket.close();
      socket = null;
    }
    window.clearTimeout(reconnectTimer);
    startSimulation();
  } else {
    stopSimulation();
    connectSocket();
  }
}

function initUi() {
  updateClock();
  window.setInterval(updateClock, 10000);

  const theme = localStorage.getItem(THEME_KEY) || 'dark';
  setTheme(theme);
  if (el.themeToggle) {
    el.themeToggle.addEventListener('click', () => setTheme(document.body.dataset.theme === 'black' ? 'dark' : 'black'));
  }

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
  el.tripResetButton.addEventListener('click', () => {
    resetTripMetrics();
    populateTripSheet();
  });
  el.tripSheetBackdrop.addEventListener('click', closeTripSheet);
  el.tripValue.addEventListener('click', openTripSheet);

  document.addEventListener('keydown', event => {
    if (event.key !== 'Escape') return;
    if (tripSheetOpen) {
      closeTripSheet();
      return;
    }
    if (settingsOpen) {
      setSettingsOpen(false);
    }
  });

  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState !== 'hidden' && !simulationMode && (!socket || socket.readyState !== WebSocket.OPEN)) {
      connectSocket();
    }
  });

  el.wsUrl.value = resolveInitialSocketUrl();
  updateSimulationUi();
  window.setTimeout(() => {
    el.tripValue.classList.add('tap-hint');
    el.tripValue.addEventListener('animationend', () => el.tripValue.classList.remove('tap-hint'), { once: true });
  }, 1200);
}

initUi();
requestAnimationFrame(drawAmbient);

const initialParams = new URLSearchParams(window.location.search);
if (initialParams.has('sim') || initialParams.get('simulation') === '1') {
  setSimulationMode(true);
} else {
  connectSocket();
}