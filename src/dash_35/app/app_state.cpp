#include "app_state.h"

#include <math.h>

#include "vehicle_profiles/vehicle_profile.h"

namespace app {

namespace {

constexpr int16_t kInitialRpm = 0;
constexpr int16_t kInitialSpeedKph = 0;
constexpr int16_t kInitialCoolantTempC = 0;
constexpr uint16_t kInitialBatteryMv = 0;
constexpr uint8_t kInitialFuelLevelPct = 0;

void integrateTripDistance(AppState &state, uint32_t now_ms, bool speed_fresh) {
  if (state.last_trip_accum_ms == 0) {
    state.last_trip_accum_ms = now_ms;
    return;
  }

  const uint32_t dt_ms = now_ms - state.last_trip_accum_ms;
  state.last_trip_accum_ms = now_ms;

  if (!speed_fresh || dt_ms <= 50U || dt_ms >= 2000U) {
    return;
  }

  const float dt_h = static_cast<float>(dt_ms) / 3600000.0f;
  state.view_state.trip.trip_distance_km += static_cast<float>(state.telemetry.speed_kph) * dt_h;
}

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
  state.last_trip_accum_ms = 0;
  state.last_speed_sample_ms = 0;
  return state;
}

void advanceSimulation(AppState &state, uint32_t now_ms) {
  const int8_t weather_temp_c = state.telemetry.weather_temp_c;
  const uint8_t weather_code = state.telemetry.weather_code;
  const bool wifi_connected = state.telemetry.wifi_connected;
  const uint16_t nearest_camera_m = state.telemetry.nearest_camera_m;
  const bool headlights_on = state.telemetry.headlights_on;

  state.previous_speed_kph = state.telemetry.speed_kph;
  updateMotionState(state);
  updateHealthState(state, now_ms);
  integrateTripDistance(state, now_ms, true);
  int16_t longitudinal_accel_mg = 0;

  if (state.last_speed_sample_ms != 0 && now_ms > state.last_speed_sample_ms) {
    const float dt_s = static_cast<float>(now_ms - state.last_speed_sample_ms) / 1000.0f;
    if (dt_s > 0.05f && dt_s < 2.0f) {
      const float delta_kph = static_cast<float>(state.telemetry.speed_kph - state.previous_speed_kph);
      const float delta_mps = delta_kph / 3.6f;
      const float accel_g = (delta_mps / dt_s) / 9.80665f;
      longitudinal_accel_mg = static_cast<int16_t>(lroundf(accel_g * 1000.0f));
    }
  }
  state.last_speed_sample_ms = now_ms;

  state.telemetry = telemetry::makeSimulatedTelemetry(
      state.telemetry.sequence + 1, now_ms, state.telemetry.rpm, state.telemetry.speed_kph,
      state.telemetry.coolant_temp_c, state.telemetry.battery_mv, state.telemetry.fuel_level_pct);
  state.telemetry.longitudinal_accel_mg = longitudinal_accel_mg;
  state.telemetry.weather_temp_c = weather_temp_c;
  state.telemetry.weather_code = weather_code;
  state.telemetry.wifi_connected = wifi_connected;
  state.telemetry.nearest_camera_m = nearest_camera_m;
  state.telemetry.headlights_on = headlights_on;
  vehicle_profiles::refreshTelemetry(state.telemetry);
  updateDashboardViewState(state.view_state, state.telemetry, state.previous_speed_kph);
}

void maintainIdleState(AppState &state, uint32_t now_ms) {
  state.telemetry.uptime_ms = now_ms;
  vehicle_profiles::refreshTelemetry(state.telemetry);
  updateDashboardViewState(state.view_state, state.telemetry, state.previous_speed_kph);
}

void accumulateTripDistance(AppState &state, uint32_t now_ms, bool speed_fresh) {
  integrateTripDistance(state, now_ms, speed_fresh);
}

}  // namespace app