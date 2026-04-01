import { startTelemetryStream, themes } from "../shared/telemetry.js";

const frame = document.getElementById("frame");
const themeSelect = document.getElementById("theme-select");
const tachRibbon = document.getElementById("tach-ribbon");
const tachSegmentCount = 20;
const tachShiftZoneStart = 16;
const maxDisplayedRpm = 5200;

const elements = {
  welcomeBanner: document.getElementById("welcome-banner"),
  modePill: document.getElementById("mode-pill"),
  modeText: document.getElementById("mode-text"),
  screenLabel: document.getElementById("screen-label"),
  rpmValue: document.getElementById("rpm-value"),
  speed: document.getElementById("speed"),
  gear: document.getElementById("gear"),
  range: document.getElementById("range"),
  rangeFill: document.getElementById("range-fill"),
  fuel: document.getElementById("fuel"),
  fuelFill: document.getElementById("fuel-fill"),
  time: document.getElementById("time"),
  date: document.getElementById("date"),
};

const maxRangeKm = 360;
const timeFormatter = new Intl.DateTimeFormat([], {
  hour: "2-digit",
  minute: "2-digit",
});
const dateFormatter = new Intl.DateTimeFormat([], {
  weekday: "short",
  day: "2-digit",
  month: "short",
});

function createTachSegments() {
  const fragment = document.createDocumentFragment();

  for (let index = 0; index < tachSegmentCount; index += 1) {
    const segment = document.createElement("span");
    segment.className = "tach-segment";
    segment.style.setProperty("--segment-height", `${24 + index * 2.1}px`);

    if (index >= tachShiftZoneStart) {
      segment.classList.add("shift-zone");
    }

    fragment.appendChild(segment);
  }

  tachRibbon.replaceChildren(fragment);
}

function applyTheme(themeName) {
  const theme = themes[themeName];

  if (!theme) {
    return;
  }

  Object.entries({
    "--bg-0": theme.bg0,
    "--bg-1": theme.bg1,
    "--bg-2": theme.bg2,
    "--bg-3": theme.bg3,
    "--accent": theme.accent,
    "--accent-strong": theme.accentStrong,
    "--accent-2": theme.accent2,
    "--accent-3": theme.accent3,
    "--danger": theme.danger,
    "--panel": theme.panel,
    "--line": theme.line,
    "--glow": theme.glow,
    "--halo": theme.halo,
    "--shift": theme.shift,
    "--tach-off": theme.tachOff,
  }).forEach(([variable, value]) => {
    document.documentElement.style.setProperty(variable, value);
  });
}

function setGaugeRpm(rpm) {
  const normalized = Math.max(0, Math.min(1, rpm / maxDisplayedRpm));
  const activeSegments = Math.round(normalized * tachSegmentCount);
  const segments = tachRibbon.querySelectorAll(".tach-segment");

  elements.rpmValue.textContent = rpm.toLocaleString();

  segments.forEach((segment, index) => {
    const isActive = index < activeSegments;
    segment.classList.toggle("active", isActive);
    segment.classList.toggle("warning", isActive && index >= tachShiftZoneStart);
  });
}

function updateClock() {
  const now = new Date();
  elements.time.textContent = timeFormatter.format(now);
  elements.date.textContent = dateFormatter.format(now);
}

function updatePreview(telemetry) {
  elements.speed.textContent = telemetry.speedKph;
  elements.gear.textContent = telemetry.gear;
  elements.range.textContent = `${telemetry.rangeKm} km`;
  elements.rangeFill.style.width = `${Math.round((telemetry.rangeKm / maxRangeKm) * 100)}%`;
  elements.fuel.textContent = `${telemetry.fuelPct}%`;
  elements.fuelFill.style.width = `${telemetry.fuelPct}%`;
  elements.screenLabel.textContent = telemetry.screen;
  elements.modePill.textContent = telemetry.driveMode === "sport" ? "Sport" : "Cruise";
  elements.modeText.textContent = telemetry.driveMode === "sport" ? "Sport" : "Cruise";

  frame.classList.toggle("sport", telemetry.driveMode === "sport");
  elements.welcomeBanner.classList.toggle(
    "visible",
    telemetry.screen === "welcome" || telemetry.screen === "boot",
  );

  setGaugeRpm(telemetry.rpm);
}

themeSelect.addEventListener("change", (event) => {
  applyTheme(event.target.value);
});

createTachSegments();
applyTheme(themeSelect.value);
updateClock();
window.setInterval(updateClock, 1000);
startTelemetryStream(updatePreview);