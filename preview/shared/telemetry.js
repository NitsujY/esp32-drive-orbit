const listeners = new Set();

const state = {
  frame: 0,
  speedKph: 0,
  coolantTempC: 84,
  batteryMv: 12600,
  fuelPct: 78,
  speedIncreasing: true,
};

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function computeDriveMode(speedKph) {
  return speedKph > 80 ? "sport" : "cruise";
}

function computeGear(speedKph) {
  if (speedKph < 12) {
    return "D1";
  }

  if (speedKph < 28) {
    return "D2";
  }

  if (speedKph < 44) {
    return "D3";
  }

  if (speedKph < 62) {
    return "D4";
  }

  if (speedKph < 82) {
    return "D5";
  }

  return "D6";
}

function computeRangeKm(fuelPct, speedKph) {
  const efficiencyFactor = clamp(1.02 - speedKph / 260, 0.72, 1.02);
  return Math.round(fuelPct * 4.6 * efficiencyFactor);
}

function computeMood(speedKph, rpm) {
  if (rpm > 4500) return "excited";
  if (speedKph > 80) return "alert";
  if (speedKph >= 30 && speedKph <= 80) return "happy";
  if (speedKph >= 10) return "warm";
  if (speedKph > 0 && speedKph < 10 && rpm < 900) return "sad";
  return "idle";
}

function computeScreen(frame, speedKph) {
  if (frame < 20) {
    return "boot";
  }

  if (frame < 50) {
    return "welcome";
  }

  if (speedKph > 80) {
    return "sport";
  }

  return frame % 140 < 70 ? "dashboard" : "health";
}

function computeTrip(frame, speedKph) {
  return {
    harshAccelerationCount: Math.max(0, Math.round((frame / 42) % 4)),
    harshBrakingCount: Math.max(0, Math.round((frame / 65) % 3)),
    smoothnessScore: clamp(Math.round(92 - speedKph / 6), 68, 95),
    speedConsistencyScore: clamp(Math.round(88 - speedKph / 9), 62, 90),
    coachingMessage:
      speedKph > 80
        ? "Sport state active. Keep inputs progressive through corner exit."
        : speedKph > 40
          ? "Cruise looks stable. Hold throttle transitions to protect economy."
          : "Warm-up window active. Avoid abrupt throttle while systems settle.",
  };
}

function nextTelemetry() {
  state.frame += 1;

  if (state.speedIncreasing) {
    state.speedKph += 3;
    if (state.speedKph >= 92) {
      state.speedIncreasing = false;
    }
  } else {
    state.speedKph -= 2;
    if (state.speedKph <= 0) {
      state.speedKph = 0;
      state.speedIncreasing = true;
    }
  }

  const rpm = clamp(
    Math.round(820 + state.speedKph * 38 + (state.speedIncreasing ? 280 : 80)),
    780,
    5200,
  );
  state.coolantTempC += state.speedIncreasing ? 1 : -1;
  state.coolantTempC = clamp(state.coolantTempC, 82, 92);

  if (state.speedIncreasing) {
    state.batteryMv = clamp(state.batteryMv - 8, 12380, 12600);
  } else {
    state.batteryMv = clamp(state.batteryMv + 4, 12380, 12600);
  }

  if (state.frame % 25 === 0) {
    state.fuelPct = clamp(state.fuelPct - 1, 12, 78);
  }

  return {
    sequence: state.frame,
    uptimeMs: state.frame * 100,
    rpm,
    speedKph: state.speedKph,
    coolantTempC: state.coolantTempC,
    batteryMv: state.batteryMv,
    fuelPct: state.fuelPct,
    gear: computeGear(state.speedKph),
    rangeKm: computeRangeKm(state.fuelPct, state.speedKph),
    driveMode: computeDriveMode(state.speedKph),
    mood: computeMood(state.speedKph, rpm),
    screen: computeScreen(state.frame, state.speedKph),
    obdConnected: true,
    trip: computeTrip(state.frame, state.speedKph),
  };
}

export function startTelemetryStream(onUpdate) {
  listeners.add(onUpdate);
  onUpdate(nextTelemetry());

  if (listeners.size === 1) {
    window.__driveOrbitTimer = window.setInterval(() => {
      const snapshot = nextTelemetry();
      listeners.forEach((listener) => listener(snapshot));
    }, 100);
  }

  return () => {
    listeners.delete(onUpdate);
    if (listeners.size === 0 && window.__driveOrbitTimer) {
      window.clearInterval(window.__driveOrbitTimer);
      window.__driveOrbitTimer = null;
    }
  };
}

export const themes = {
  orbital_sync: {
    bg0: "#050910",
    bg1: "#111a31",
    bg2: "#1f264f",
    bg3: "#08121e",
    accent: "#86ffe1",
    accentStrong: "#d7f8ff",
    accent2: "#71cfff",
    accent3: "#ff5fd2",
    danger: "#ffb14b",
    panel: "rgba(10, 16, 34, 0.78)",
    line: "rgba(134, 255, 225, 0.16)",
    glow: "rgba(134, 255, 225, 0.18)",
    halo: "rgba(113, 207, 255, 0.18)",
    shift: "#ff5fd2",
    tachOff: "rgba(255, 255, 255, 0.08)",
  },
  aurora_grid: {
    bg0: "#041014",
    bg1: "#10233a",
    bg2: "#0f3e49",
    bg3: "#07161a",
    accent: "#8effb7",
    accentStrong: "#f0fff7",
    accent2: "#5fdcff",
    accent3: "#67f6ff",
    danger: "#ffc857",
    panel: "rgba(7, 20, 28, 0.76)",
    line: "rgba(142, 255, 183, 0.14)",
    glow: "rgba(142, 255, 183, 0.2)",
    halo: "rgba(95, 220, 255, 0.16)",
    shift: "#67f6ff",
    tachOff: "rgba(255, 255, 255, 0.07)",
  },
  afterburn_glow: {
    bg0: "#10070a",
    bg1: "#25111c",
    bg2: "#352142",
    bg3: "#16090c",
    accent: "#ffb14b",
    accentStrong: "#ffe5c2",
    accent2: "#ff7b5f",
    accent3: "#ff55c8",
    danger: "#ffdb6e",
    panel: "rgba(24, 10, 12, 0.78)",
    line: "rgba(255, 177, 75, 0.16)",
    glow: "rgba(255, 177, 75, 0.18)",
    halo: "rgba(255, 85, 200, 0.14)",
    shift: "#ff55c8",
    tachOff: "rgba(255, 255, 255, 0.07)",
  },
};