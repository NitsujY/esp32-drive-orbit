/*
  Unified scenario schema for one shared orb + dash scene.

  Each preset describes the whole drive context instead of a single feature:

  {
    id,
    title,
    tag,
    summary,
    drive: { mode, gear, rangeKm, fuelPct },
    trip: { score, grade, smoothness, harshAccelCount, harshBrakeCount, efficiencyKml, lifetimeEfficiencyKml },
    environment: {
      icon, tempC, advisory, uvIndex, uvRisk, aqi, pm25, pm10,
      isDark, sunrise, sunset, updatedMinsAgo
    },
    fuelPrice: { pricePerL, station, distanceKm, trend, avg7d, low14d, high14d },
    speedCamera: { active, distanceM, speedLimitKph, currentSpeedKph, camerasInArea, updatedMinsAgo },
    idle: { minutes, budgetMinutes },
  }

  Renderers derive orb emotion, ring, alerts, and dash detail cards from this
  shared state so multiple features can coexist in the same composed screen.
*/

const SCENARIOS = [
  {
    id: 1,
    title: "Daily Commute",
    tag: "Day",
    summary: "Driver score + efficiency + AQI + UV + fuel price on one calm daytime screen.",
    drive: { mode: "Cruise", gear: "D4", speedKph: 58, rpm: 2380, rangeKm: 280, fuelPct: 62 },
    trip: {
      score: 87,
      grade: "A",
      smoothness: 68,
      harshAccelCount: 2,
      harshBrakeCount: 0,
      efficiencyKml: 14.2,
      lifetimeEfficiencyKml: 16.0,
    },
    environment: {
      icon: "☀",
      tempC: 28,
      advisory: "",
      uvIndex: 7,
      uvRisk: "High",
      aqi: 142,
      pm25: 58,
      pm10: 92,
      isDark: false,
      sunrise: "05:41",
      sunset: "18:32",
      updatedMinsAgo: 2,
    },
    fuelPrice: {
      pricePerL: 158,
      station: "ENEOS",
      distanceKm: 1.2,
      trend: "GOOD DAY ↓",
      avg7d: 162,
      low14d: 155,
      high14d: 168,
    },
    speedCamera: {
      active: false,
      distanceM: null,
      speedLimitKph: null,
      currentSpeedKph: null,
      camerasInArea: 3,
      updatedMinsAgo: 5,
    },
    idle: { minutes: 1.2, budgetMinutes: 5 },
  },
  {
    id: 2,
    title: "Rain + Camera",
    tag: "Wet",
    summary: "Weather advisory, active speed camera alert, and safety-first orb reaction in one scene.",
    drive: { mode: "Cruise", gear: "D4", speedKph: 58, rpm: 2640, rangeKm: 246, fuelPct: 54 },
    trip: {
      score: 79,
      grade: "A",
      smoothness: 61,
      harshAccelCount: 1,
      harshBrakeCount: 1,
      efficiencyKml: 13.6,
      lifetimeEfficiencyKml: 15.8,
    },
    environment: {
      icon: "🌧",
      tempC: 18,
      advisory: "WET ROADS",
      uvIndex: 1,
      uvRisk: "Low",
      aqi: 61,
      pm25: 18,
      pm10: 29,
      isDark: false,
      sunrise: "05:41",
      sunset: "18:32",
      updatedMinsAgo: 1,
    },
    fuelPrice: {
      pricePerL: 160,
      station: "Idemitsu",
      distanceKm: 0.9,
      trend: "NORMAL",
      avg7d: 159,
      low14d: 155,
      high14d: 168,
    },
    speedCamera: {
      active: true,
      distanceM: 350,
      speedLimitKph: 60,
      currentSpeedKph: 58,
      camerasInArea: 3,
      updatedMinsAgo: 3,
    },
    idle: { minutes: 0.4, budgetMinutes: 5 },
  },
  {
    id: 3,
    title: "Evening Refuel",
    tag: "Night",
    summary: "Fuel-price decision, auto dark mode, and a stable evening orb state on one shared UI.",
    drive: { mode: "Cruise", gear: "D3", speedKph: 42, rpm: 1980, rangeKm: 182, fuelPct: 26 },
    trip: {
      score: 91,
      grade: "S",
      smoothness: 74,
      harshAccelCount: 0,
      harshBrakeCount: 0,
      efficiencyKml: 15.8,
      lifetimeEfficiencyKml: 16.4,
    },
    environment: {
      icon: "☾",
      tempC: 22,
      advisory: "",
      uvIndex: 0,
      uvRisk: "None",
      aqi: 42,
      pm25: 10,
      pm10: 18,
      isDark: true,
      sunrise: "05:41",
      sunset: "18:32",
      updatedMinsAgo: 6,
    },
    fuelPrice: {
      pricePerL: 157,
      station: "Cosmo",
      distanceKm: 1.4,
      trend: "GOOD DAY ↓",
      avg7d: 161,
      low14d: 155,
      high14d: 168,
    },
    speedCamera: {
      active: false,
      distanceM: null,
      speedLimitKph: null,
      currentSpeedKph: null,
      camerasInArea: 2,
      updatedMinsAgo: 8,
    },
    idle: { minutes: 1.9, budgetMinutes: 5 },
  },
  {
    id: 4,
    title: "After-Work Idle",
    tag: "Idle",
    summary: "Idle budget breach while AQI and weather stay visible without crowding the main dash.",
    drive: { mode: "Cruise", gear: "P", speedKph: 0, rpm: 820, rangeKm: 204, fuelPct: 44 },
    trip: {
      score: 72,
      grade: "B",
      smoothness: 55,
      harshAccelCount: 2,
      harshBrakeCount: 1,
      efficiencyKml: 12.9,
      lifetimeEfficiencyKml: 15.6,
    },
    environment: {
      icon: "⛅",
      tempC: 24,
      advisory: "",
      uvIndex: 2,
      uvRisk: "Low",
      aqi: 101,
      pm25: 36,
      pm10: 55,
      isDark: false,
      sunrise: "05:41",
      sunset: "18:32",
      updatedMinsAgo: 4,
    },
    fuelPrice: {
      pricePerL: 163,
      station: "ENEOS",
      distanceKm: 1.2,
      trend: "HIGH TODAY ↑",
      avg7d: 160,
      low14d: 155,
      high14d: 168,
    },
    speedCamera: {
      active: false,
      distanceM: null,
      speedLimitKph: null,
      currentSpeedKph: null,
      camerasInArea: 3,
      updatedMinsAgo: 7,
    },
    idle: { minutes: 6.2, budgetMinutes: 5 },
  },
];

const orbShell = document.getElementById("orb-shell");
const orbMoodRing = document.getElementById("orb-mood-ring");
const orbFace = document.getElementById("orb-face");
const orbScore = document.getElementById("orb-score");
const orbGrade = document.getElementById("orb-grade");
const orbWeather = document.getElementById("orb-weather");
const orbAdvisory = document.getElementById("orb-advisory");
const orbHaze = document.getElementById("orb-haze");
const orbNote = document.getElementById("orb-note");
const orbContent = document.querySelector(".orb-content");

const dashShell = document.getElementById("dash-shell");
const tachRibbon = document.getElementById("tach-ribbon");
const dtbCam = document.getElementById("dtb-cam");
const dtbAlert = document.getElementById("dtb-alert");
const dtbIcons = document.getElementById("dtb-icons");
const dtbMode = document.getElementById("dtb-mode");
const driveSpeed = document.getElementById("drive-speed");
const driveRpm = document.getElementById("drive-rpm");
const dbbGear = document.getElementById("dbb-gear");
const dbbRange = document.getElementById("dbb-range");
const dbbFuel = document.getElementById("dbb-fuel");
const fuelFill = document.getElementById("fuel-fill");
const dashNote = document.getElementById("dash-note");
const dtbTime = document.getElementById("dtb-time");
const featureGrid = document.getElementById("feature-grid");
const cycleBadge = document.getElementById("cycle-badge");

let autoCycleTimer = null;
let autoCycleIndex = 0;
let userPaused = false;

const CYCLE_INTERVAL_MS = 6000;
const tachSegmentCount = 20;
const tachShiftZoneStart = 16;
const maxDisplayedRpm = 5200;

function updateClock() {
  const now = new Date();
  const hours = String(now.getHours()).padStart(2, "0");
  const minutes = String(now.getMinutes()).padStart(2, "0");
  dtbTime.textContent = `${hours}:${minutes}`;
}

function formatPriceYen(value) {
  return `¥${value}/L`;
}

function buildCard(label, value, sub, accent) {
  return { label, value, sub, accent };
}

function buildIcon(mark, text, accent) {
  return { mark, text, accent };
}

function createTachSegments() {
  const fragment = document.createDocumentFragment();

  for (let index = 0; index < tachSegmentCount; index += 1) {
    const segment = document.createElement("span");
    segment.className = "tach-segment";
    segment.style.setProperty("--segment-height", `${18 + index * 1.55}px`);

    if (index >= tachShiftZoneStart) {
      segment.classList.add("shift-zone");
    }

    fragment.appendChild(segment);
  }

  tachRibbon.replaceChildren(fragment);
}

function setGaugeRpm(rpm) {
  const normalized = Math.max(0, Math.min(1, rpm / maxDisplayedRpm));
  const activeSegments = Math.round(normalized * tachSegmentCount);
  const segments = tachRibbon.querySelectorAll(".tach-segment");

  segments.forEach((segment, index) => {
    const isActive = index < activeSegments;
    segment.classList.toggle("active", isActive);
    segment.classList.toggle("warning", isActive && index >= tachShiftZoneStart);
  });
}

function resolveOrbState(scenario) {
  if (scenario.speedCamera.active && scenario.speedCamera.distanceM <= 500) {
    return {
      face: "! !",
      ring: "#ff5060",
      ringGlow: "rgba(255,80,96,0.42)",
      ringAnim: "ring-fast",
      score: String(scenario.trip.score),
      grade: `cam ${scenario.speedCamera.distanceM}m`,
      weather: `${scenario.environment.icon} ${scenario.environment.tempC}°C`,
      advisory: "slow down",
      haze: scenario.environment.aqi > 100 ? 0.18 : 0,
      note: "speed camera overrides other orb states",
    };
  }

  if (scenario.idle.minutes > scenario.idle.budgetMinutes) {
    return {
      face: "¬_¬",
      ring: "#ffb14b",
      ringGlow: "rgba(255,177,75,0.34)",
      ringAnim: "",
      score: String(scenario.trip.score),
      grade: "bored...",
      weather: `${scenario.environment.icon} ${scenario.environment.tempC}°C`,
      advisory: "engine idling",
      haze: scenario.environment.aqi > 100 ? 0.22 : 0,
      note: "idle budget exceeded",
    };
  }

  if (scenario.environment.advisory) {
    return {
      face: "ˊ_ˋ",
      ring: "#71cfff",
      ringGlow: "rgba(113,207,255,0.34)",
      ringAnim: "",
      score: String(scenario.trip.score),
      grade: `${scenario.trip.grade} careful`,
      weather: `${scenario.environment.icon} ${scenario.environment.tempC}°C`,
      advisory: scenario.environment.advisory,
      haze: scenario.environment.aqi > 100 ? 0.2 : 0,
      note: "weather advisory is active",
    };
  }

  if (scenario.trip.score >= 85) {
    return {
      face: "◡‿◡",
      ring: scenario.environment.isDark ? "#c8a4f0" : "#4eff91",
      ringGlow: scenario.environment.isDark ? "rgba(200,164,240,0.34)" : "rgba(78,255,145,0.38)",
      ringAnim: "",
      score: String(scenario.trip.score),
      grade: `smooth ${scenario.trip.grade}`,
      weather: scenario.environment.isDark
        ? `${scenario.environment.icon} ${scenario.environment.tempC}°C`
        : scenario.environment.uvIndex > 0
          ? `${scenario.environment.icon} UV ${scenario.environment.uvIndex}`
          : `${scenario.environment.icon} ${scenario.environment.tempC}°C`,
      advisory: "",
      haze: scenario.environment.aqi > 100 ? 0.2 : 0,
      note: scenario.environment.isDark ? "night mode with strong trip score" : "healthy trip score and ambient weather",
    };
  }

  return {
    face: "◡_◡",
    ring: scenario.environment.isDark ? "#c8a4f0" : "#86ffe1",
    ringGlow: scenario.environment.isDark ? "rgba(200,164,240,0.34)" : "rgba(134,255,225,0.34)",
    ringAnim: "",
    score: String(scenario.trip.score),
    grade: `steady ${scenario.trip.grade}`,
    weather: `${scenario.environment.icon} ${scenario.environment.tempC}°C`,
    advisory: "",
    haze: scenario.environment.aqi > 100 ? 0.18 : 0,
    note: "balanced combined state",
  };
}

function resolveDashAlert(scenario) {
  if (scenario.idle.minutes > scenario.idle.budgetMinutes) {
    const over = (scenario.idle.minutes - scenario.idle.budgetMinutes).toFixed(1);
    return `IDLE ${scenario.idle.minutes.toFixed(1)}m (+${over}m)`;
  }

  if (scenario.environment.advisory) {
    return `⚠ ${scenario.environment.advisory} — take care`;
  }

  if (scenario.fuelPrice.trend === "GOOD DAY ↓" && scenario.drive.fuelPct <= 30) {
    return "GOOD DAY TO FILL";
  }

  if (scenario.environment.isDark) {
    return `AUTO DARK — sunset ${scenario.environment.sunset}`;
  }

  return "";
}

function resolveDashIcons(scenario) {
  const driveFeel =
    scenario.trip.score >= 90 ? "Smooth" :
    scenario.trip.score >= 80 ? "Good" :
    scenario.trip.score >= 70 ? "Steady" : "Care";

  const airState =
    scenario.environment.aqi > 100 ? "AQI" :
    scenario.environment.uvIndex >= 6 ? "UV" : "Air";

  const focusState = scenario.speedCamera.active
    ? `Cam`
    : scenario.idle.minutes > scenario.idle.budgetMinutes
      ? "Idle"
      : scenario.environment.advisory
        ? "Weather"
        : scenario.environment.isDark
          ? "Night"
          : "Clear";

  return [
    buildIcon("◈", driveFeel, scenario.trip.score >= 85 ? "green" : "amber"),
    buildIcon(scenario.environment.aqi > 100 ? "≈" : scenario.environment.icon, airState, scenario.environment.aqi > 100 ? "red" : scenario.environment.uvIndex >= 6 ? "amber" : "blue"),
    buildIcon("⛽", scenario.fuelPrice.trend === "GOOD DAY ↓" ? "Fill" : scenario.fuelPrice.trend === "HIGH TODAY ↑" ? "Wait" : "Fuel", scenario.fuelPrice.trend === "GOOD DAY ↓" ? "green" : scenario.fuelPrice.trend === "HIGH TODAY ↑" ? "amber" : "white"),
    buildIcon(scenario.speedCamera.active ? "!" : scenario.environment.isDark ? "☾" : scenario.idle.minutes > scenario.idle.budgetMinutes ? "…" : "•", focusState, scenario.speedCamera.active ? "red" : scenario.idle.minutes > scenario.idle.budgetMinutes || scenario.environment.advisory ? "amber" : scenario.environment.isDark ? "purple" : "white"),
  ];
}

function buildTopbarIcon(icon) {
  const element = document.createElement("span");
  element.className = `dtb-icon ${icon.accent}`;
  element.innerHTML = `<span class="dtb-icon-mark">${icon.mark}</span><span>${icon.text}</span>`;
  return element;
}

function renderOrb(scenario) {
  const orb = resolveOrbState(scenario);
  orbContent.classList.add("fading");

  setTimeout(() => {
    orbShell.classList.remove("ring-fast", "ring-rainbow");
    if (orb.ringAnim) {
      orbShell.classList.add(orb.ringAnim);
    }

    const rootStyle = document.documentElement.style;
    rootStyle.setProperty("--ring", orb.ring);
    rootStyle.setProperty("--ring-glow", orb.ringGlow);

    orbMoodRing.style.borderColor = orb.ring;
    orbMoodRing.style.boxShadow = `0 0 14px ${orb.ringGlow}, 0 0 32px ${orb.ringGlow}, inset 0 0 8px ${orb.ringGlow}`;

    orbFace.textContent = orb.face;
    orbFace.style.textShadow = `0 0 24px ${orb.ring}`;
    orbFace.classList.remove("pop");
    void orbFace.offsetWidth;
    orbFace.classList.add("pop");

    orbScore.textContent = orb.score;
    orbGrade.textContent = orb.grade;
    orbWeather.textContent = orb.weather;
    orbAdvisory.textContent = orb.advisory;
    orbNote.textContent = orb.note;

    orbHaze.style.setProperty("--haze-base", orb.haze);
    if (orb.haze > 0) {
      orbHaze.style.background = `radial-gradient(circle, rgba(180,140,210,${orb.haze * 0.5}), rgba(150,100,190,${orb.haze}))`;
      orbHaze.style.opacity = String(orb.haze);
    } else {
      orbHaze.style.opacity = "0";
    }

    orbContent.classList.remove("fading");
  }, 220);
}

function renderDash(scenario) {
  dtbCam.classList.add("fading");
  dtbAlert.classList.add("fading");
  dtbIcons.classList.add("fading");

  setTimeout(() => {
    const icons = resolveDashIcons(scenario);
    const fragment = document.createDocumentFragment();
    icons.forEach((icon) => fragment.appendChild(buildTopbarIcon(icon)));
    dtbIcons.replaceChildren(fragment);

    dtbAlert.textContent = resolveDashAlert(scenario);
    if (scenario.speedCamera.active) {
      dtbCam.textContent = `CAM ${scenario.speedCamera.distanceM}m`;
      dtbCam.classList.add("visible");
    } else {
      dtbCam.textContent = "";
      dtbCam.classList.remove("visible");
    }

    dashShell.classList.toggle("dark-mode", scenario.environment.isDark);
    dtbMode.textContent = scenario.drive.mode;
    driveSpeed.textContent = String(scenario.drive.speedKph);
    driveRpm.textContent = scenario.drive.rpm.toLocaleString();
    setGaugeRpm(scenario.drive.rpm);
    dbbGear.textContent = scenario.drive.gear;
    dbbRange.textContent = `${scenario.drive.rangeKm} km`;
    dbbFuel.textContent = `${scenario.drive.fuelPct}%`;
    fuelFill.style.width = `${scenario.drive.fuelPct}%`;
    dashNote.textContent = `${scenario.title.toLowerCase()} combined state`;

    dtbCam.classList.remove("fading");
    dtbAlert.classList.remove("fading");
    dtbIcons.classList.remove("fading");
  }, 220);
}

function activateScenario(id, fromUser) {
  if (fromUser) {
    userPaused = true;
    cycleBadge.textContent = "Paused";
    cycleBadge.style.opacity = "0.55";
    if (autoCycleTimer) {
      clearInterval(autoCycleTimer);
      autoCycleTimer = null;
    }
  }

  const scenario = SCENARIOS.find((entry) => entry.id === id);
  if (!scenario) {
    return;
  }

  document.querySelectorAll(".feature-card").forEach((card) => {
    card.classList.toggle("active", Number(card.dataset.id) === id);
  });

  renderOrb(scenario);
  renderDash(scenario);
}

function buildScenarioCard(scenario) {
  const card = document.createElement("button");
  card.className = "feature-card";
  card.dataset.id = scenario.id;
  card.setAttribute("aria-label", scenario.title);

  const tags = [
    "score",
    "efficiency",
    "aqi",
    scenario.environment.uvIndex > 0 ? "uv" : null,
    scenario.fuelPrice ? "fuel" : null,
    scenario.speedCamera.active ? "camera" : null,
    scenario.environment.advisory ? "weather" : null,
    scenario.environment.isDark ? "night" : null,
    scenario.idle.minutes > scenario.idle.budgetMinutes ? "idle" : null,
  ].filter(Boolean).slice(0, 4);

  card.innerHTML = `
    <div class="fc-header">
      <span class="fc-num">${String(scenario.id).padStart(2, "0")}</span>
      <span class="fc-tag ${scenario.environment.isDark ? "online" : "offline"}">${scenario.tag}</span>
    </div>
    <div class="fc-title">${scenario.title}</div>
    <div class="fc-desc">${scenario.summary}</div>
    <div class="fc-affects">${tags.map((tag) => `<span class="fc-device dash">${tag}</span>`).join("")}</div>
  `;

  card.addEventListener("click", () => activateScenario(scenario.id, true));
  return card;
}

function buildScenarioCards() {
  const fragment = document.createDocumentFragment();
  SCENARIOS.forEach((scenario) => fragment.appendChild(buildScenarioCard(scenario)));
  featureGrid.replaceChildren(fragment);
}

function startAutoCycle() {
  autoCycleTimer = setInterval(() => {
    if (userPaused) {
      return;
    }

    autoCycleIndex = (autoCycleIndex + 1) % SCENARIOS.length;
    activateScenario(SCENARIOS[autoCycleIndex].id, false);
  }, CYCLE_INTERVAL_MS);
}

cycleBadge.addEventListener("click", () => {
  userPaused = false;
  cycleBadge.textContent = "Auto-cycling";
  cycleBadge.style.opacity = "";
  if (!autoCycleTimer) {
    startAutoCycle();
  }
});

buildScenarioCards();
createTachSegments();
updateClock();
setInterval(updateClock, 10000);
activateScenario(SCENARIOS[0].id, false);
startAutoCycle();
