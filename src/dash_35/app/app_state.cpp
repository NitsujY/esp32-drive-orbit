#include "app_state.h"

namespace app {

namespace {

constexpr int16_t kInitialRpm = 900;
constexpr int16_t kInitialSpeedKph = 0;
constexpr int16_t kInitialCoolantTempC = 84;
constexpr uint16_t kInitialBatteryMv = 12600;
constexpr uint8_t kInitialFuelLevelPct = 78;

void updateMotionState(AppState &state) {
  if (state.speed_increasing) {
    state.telemetry.speed_kph += 3;
    if (state.telemetry.speed_kph >= 92) {
      state.speed_increasing = false;
    }
  } else {
    state.telemetry.speed_kph -= 2;
    if (state.telemetry.speed_kph <= 0) {
      state.telemetry.speed_kph = 0;
      state.speed_increasing = true;
    }
  }

  state.telemetry.rpm = 850 + (state.telemetry.speed_kph * 42);
}

void updateHealthState(AppState &state, uint32_t now_ms) {
  if (state.speed_increasing && state.telemetry.coolant_temp_c < 92) {
    ++state.telemetry.coolant_temp_c;
  }

  if (!state.speed_increasing && state.telemetry.coolant_temp_c > 82) {
    --state.telemetry.coolant_temp_c;
  }

  if (state.speed_increasing && state.telemetry.battery_mv > 12380) {
    state.telemetry.battery_mv -= 8;
  } else if (!state.speed_increasing && state.telemetry.battery_mv < 12600) {
    state.telemetry.battery_mv += 4;
  }

  if ((now_ms / 2500U) % 2U == 0U && state.telemetry.fuel_level_pct > 12) {
    --state.telemetry.fuel_level_pct;
  }
}

}  // namespace

AppState makeInitialState() {
  AppState state{};
  state.telemetry = telemetry::makeSimulatedTelemetry(0, 0, kInitialRpm, kInitialSpeedKph,
                                                      kInitialCoolantTempC, kInitialBatteryMv,
                                                      kInitialFuelLevelPct);
  state.view_state = makeInitialDashboardViewState();
  state.psram_detected = false;
  state.psram_size_bytes = 0;
  state.speed_increasing = true;
  state.previous_speed_kph = state.telemetry.speed_kph;
  return state;
}

void advanceSimulation(AppState &state, uint32_t now_ms) {
  state.previous_speed_kph = state.telemetry.speed_kph;
  updateMotionState(state);
  updateHealthState(state, now_ms);

  state.telemetry = telemetry::makeSimulatedTelemetry(
      state.telemetry.sequence + 1, now_ms, state.telemetry.rpm, state.telemetry.speed_kph,
      state.telemetry.coolant_temp_c, state.telemetry.battery_mv, state.telemetry.fuel_level_pct);
  updateDashboardViewState(state.view_state, state.telemetry, state.previous_speed_kph);
}

}  // namespace app