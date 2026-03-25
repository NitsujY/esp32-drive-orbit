import { startTelemetryStream } from "../shared/telemetry.js";

const shell = document.getElementById("orb-shell");
const screen = shell.querySelector(".orb-screen");

const elements = {
  face: document.getElementById("orb-face"),
  label: document.getElementById("orb-label"),
  subtitle: document.getElementById("orb-subtitle"),
  speed: document.getElementById("orb-speed"),
  mode: document.getElementById("mode"),
  mood: document.getElementById("mood"),
  reaction: document.getElementById("reaction"),
};

function reactionForTelemetry(telemetry) {
  if (telemetry.driveMode === "sport") {
    return {
      face: "✦",
      label: "Attack Vector",
      subtitle: "Sport state active. Persona intensity raised for spirited driving.",
      reaction: "sport-reaction",
    };
  }

  if (telemetry.mood === "alert") {
    return {
      face: "◉",
      label: "Sharp Focus",
      subtitle: "Driver state looks energetic. Maintaining high-attention companion mode.",
      reaction: "alert",
    };
  }

  if (telemetry.mood === "warm") {
    return {
      face: "◌",
      label: "Calm Drive",
      subtitle: "Cruise is smooth. Companion stays supportive and low-noise.",
      reaction: "calm-drive",
    };
  }

  return {
    face: "·",
    label: "Idle Orbit",
    subtitle: "Warm-up sequence ready. Awaiting confident movement.",
    reaction: "idle",
  };
}

function render(telemetry) {
  const reaction = reactionForTelemetry(telemetry);

  screen.classList.toggle("sport", telemetry.driveMode === "sport");
  elements.face.textContent = reaction.face;
  elements.label.textContent = reaction.label;
  elements.subtitle.textContent = reaction.subtitle;
  elements.speed.textContent = `${telemetry.speedKph} km/h`;
  elements.mode.textContent = telemetry.driveMode;
  elements.mood.textContent = telemetry.mood;
  elements.reaction.textContent = reaction.reaction;
}

startTelemetryStream(render);