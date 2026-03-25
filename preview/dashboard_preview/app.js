import { startTelemetryStream, themes } from "../shared/telemetry.js";

const frame = document.getElementById("frame");
const themeSelect = document.getElementById("theme-select");

const elements = {
  welcomeBanner: document.getElementById("welcome-banner"),
  modePill: document.getElementById("mode-pill"),
  modeText: document.getElementById("mode-text"),
  screenLabel: document.getElementById("screen-label"),
  speed: document.getElementById("speed"),
  rpmArc: document.getElementById("rpm-arc"),
  coolant: document.getElementById("coolant"),
  battery: document.getElementById("battery"),
  fuel: document.getElementById("fuel"),
  fuelFill: document.getElementById("fuel-fill"),
};

function applyTheme(themeName) {
  const theme = themes[themeName];
  document.documentElement.style.setProperty("--accent", theme.accent);
  document.documentElement.style.setProperty("--accent-strong", theme.accentStrong);
  document.documentElement.style.setProperty("--danger", theme.danger);
  document.documentElement.style.setProperty("--panel", theme.panel);
  document.documentElement.style.setProperty("--line", theme.line);
}

function setGaugeRpm(rpm) {
  const normalized = Math.max(0, Math.min(1, rpm / 2600));
  const offset = 100 - normalized * 100;
  elements.rpmArc.style.strokeDashoffset = `${offset}`;
}

function updatePreview(telemetry) {
  elements.speed.textContent = telemetry.speedKph;
  elements.coolant.textContent = `${telemetry.coolantTempC}C`;
  elements.battery.textContent = `${(telemetry.batteryMv / 1000).toFixed(2)}V`;
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

applyTheme(themeSelect.value);
startTelemetryStream(updatePreview);