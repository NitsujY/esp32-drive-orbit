const shell = document.getElementById("orb-shell");
const screen = shell.querySelector(".orb-screen");
const stateSelect = document.getElementById("state-select");
const verificationStateDurationMs = 5000;
const verificationStates = [
  {
    state: "idle",
    face: "·",
    label: "Idle",
    subtitle: "Waiting quietly for motion.",
    pulse: "standby",
  },
  {
    state: "ready",
    face: "·",
    label: "Ready",
    subtitle: "Quiet support mode. No duplicate driving data shown here.",
    pulse: "steady",
  },
  {
    state: "aware",
    face: "◌",
    label: "Aware",
    subtitle: "Tracking motion and watching for anything unusual.",
    pulse: "watching",
  },
  {
    state: "focus",
    face: "✦",
    label: "Focus",
    subtitle: "Heightened attention. Companion keeps reactions short and direct.",
    pulse: "intense",
  },
];
const burstDurations = {
  idle: 4800,
  ready: 3200,
  aware: 2200,
  focus: 1500,
};

const elements = {
  face: document.getElementById("orb-face"),
  label: document.getElementById("orb-label"),
  subtitle: document.getElementById("orb-subtitle"),
  pulse: document.getElementById("orb-pulse"),
};

let activeState = "idle";
let burstTimer = null;
let burstResetTimer = null;
let verificationIndex = 0;
let verificationTimer = null;

function findState(state) {
  return verificationStates.find((candidate) => candidate.state === state) ?? verificationStates[0];
}

function scheduleBurst(state) {
  if (burstTimer) {
    window.clearInterval(burstTimer);
  }

  if (burstResetTimer) {
    window.clearTimeout(burstResetTimer);
  }

  burstTimer = window.setInterval(() => {
    screen.classList.add("is-bursting");
    burstResetTimer = window.setTimeout(() => {
      screen.classList.remove("is-bursting");
    }, 460);
  }, burstDurations[state]);
}

function setState(state) {
  if (activeState === state) {
    return;
  }

  screen.classList.remove(`state-${activeState}`);
  activeState = state;
  screen.classList.add(`state-${activeState}`);
  scheduleBurst(state);
}

function renderState(reaction) {
  setState(reaction.state);
  screen.classList.toggle("sport", reaction.state === "focus");
  elements.face.textContent = reaction.face;
  elements.label.textContent = reaction.label;
  elements.subtitle.textContent = reaction.subtitle;
  elements.pulse.textContent = reaction.pulse;
}

function advanceVerificationState() {
  renderState(verificationStates[verificationIndex]);
  verificationIndex = (verificationIndex + 1) % verificationStates.length;
}

function startAutoCycle() {
  if (verificationTimer) {
    window.clearInterval(verificationTimer);
  }

  verificationTimer = window.setInterval(advanceVerificationState, verificationStateDurationMs);
}

stateSelect.addEventListener("change", (event) => {
  const selectedState = event.target.value;

  if (selectedState === "auto") {
    advanceVerificationState();
    startAutoCycle();
    return;
  }

  if (verificationTimer) {
    window.clearInterval(verificationTimer);
    verificationTimer = null;
  }

  renderState(findState(selectedState));
});

screen.classList.add("state-idle");
scheduleBurst(activeState);
advanceVerificationState();
startAutoCycle();