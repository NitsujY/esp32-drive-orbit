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

function computeMood(speedKph) {
  if (speedKph < 10) {
    return "idle";
  }

  if (speedKph < 45) {
    return "warm";
  }

  return "alert";
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

  const rpm = Math.round(780 + state.speedKph * 18 + (state.speedIncreasing ? 70 : 0));
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
    mood: computeMood(state.speedKph),
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
  minimalist_future: {
    accent: "#7df9c6",
    accentStrong: "#c8fff0",
    danger: "#ff8474",
    panel: "rgba(7, 20, 28, 0.82)",
    line: "rgba(145, 212, 228, 0.18)",
  },
  midnight_sport: {
    accent: "#ff7357",
    accentStrong: "#ffd6c8",
    danger: "#ffe66d",
    panel: "rgba(30, 10, 10, 0.84)",
    line: "rgba(255, 115, 87, 0.22)",
  },
  neon_circuit: {
    accent: "#70f0ff",
    accentStrong: "#effdff",
    danger: "#ff5cb8",
    panel: "rgba(12, 11, 30, 0.84)",
    line: "rgba(112, 240, 255, 0.2)",
  },
};