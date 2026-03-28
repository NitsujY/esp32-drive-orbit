#include "dashboard_state.h"

#include <stdio.h>

namespace app {

namespace {

constexpr uint32_t kBootDurationMs = 1500;
constexpr uint32_t kWelcomeDurationMs = 5000;

uint8_t clampScore(int value) {
  return static_cast<uint8_t>(constrain(value, 0, 100));
}

void updateTripMetrics(TripMetrics &trip,
                       const telemetry::DashboardTelemetry &telemetry,
                       int16_t previous_speed_kph) {
  const int16_t speed_delta = telemetry.speed_kph - previous_speed_kph;

  if (speed_delta >= 4 && trip.harsh_acceleration_count < 255) {
    ++trip.harsh_acceleration_count;
  }

  if (speed_delta <= -4 && trip.harsh_braking_count < 255) {
    ++trip.harsh_braking_count;
  }

  trip.smoothness_score = clampScore(96 - abs(speed_delta) * 8 - telemetry.speed_kph / 10);
  trip.speed_consistency_score =
      clampScore(92 - abs(speed_delta) * 10 - telemetry.speed_kph / 12);

  if (telemetry.fuel_level_pct <= 18) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Fuel reserve is low. Keep inputs smooth and plan a stop.");
    return;
  }

  if (telemetry.coolant_temp_c >= 90) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Coolant is elevated. Ease the pace and watch system load.");
    return;
  }

  if (telemetry.drive_mode == telemetry::DriveMode::Sport) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Sport state active. Keep throttle and braking inputs progressive.");
    return;
  }

  if (telemetry.speed_kph >= 45) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Cruise looks stable. Hold steady pacing for better consistency.");
    return;
  }

  snprintf(trip.coaching_message, sizeof(trip.coaching_message),
           "Warm-up window active. Build pace gradually while systems settle.");
}

DashboardScreen resolveScreen(const DashboardViewState &view_state,
                              const telemetry::DashboardTelemetry &telemetry) {
  (void)view_state;

  if (telemetry.uptime_ms < kBootDurationMs) {
    return DashboardScreen::Boot;
  }

  if (telemetry.uptime_ms < kWelcomeDurationMs) {
    return DashboardScreen::Welcome;
  }

  return DashboardScreen::Dashboard;
}

}  // namespace

DashboardViewState makeInitialDashboardViewState() {
  DashboardViewState state{};
  state.active_screen = DashboardScreen::Boot;
  state.welcome_visible = true;
  state.health_attention = false;
  state.obd_connection_state = ObdConnectionState::Disconnected;
  resetTripMetrics(state, "System booting. Preparing welcome sequence.");
  return state;
}

void resetTripMetrics(DashboardViewState &view_state, const char *message) {
  view_state.trip.trip_distance_km = 0.0f;
  view_state.trip.harsh_acceleration_count = 0;
  view_state.trip.harsh_braking_count = 0;
  view_state.trip.smoothness_score = 100;
  view_state.trip.speed_consistency_score = 100;
  snprintf(view_state.trip.coaching_message, sizeof(view_state.trip.coaching_message), "%s",
           message != nullptr ? message : "Trip reset. Waiting for live vehicle data.");
}

void updateDashboardViewState(DashboardViewState &view_state,
                              const telemetry::DashboardTelemetry &telemetry,
                              int16_t previous_speed_kph) {
  updateTripMetrics(view_state.trip, telemetry, previous_speed_kph);
  view_state.welcome_visible = telemetry.uptime_ms < kWelcomeDurationMs;
  view_state.health_attention = telemetry.coolant_temp_c >= 90 || telemetry.fuel_level_pct <= 18;
  view_state.active_screen = resolveScreen(view_state, telemetry);
}

const char *dashboardScreenName(DashboardScreen screen) {
  switch (screen) {
    case DashboardScreen::Boot:
      return "boot";
    case DashboardScreen::Welcome:
      return "welcome";
    case DashboardScreen::Dashboard:
      return "dashboard";
    case DashboardScreen::Trip:
      return "trip";
    case DashboardScreen::Health:
      return "health";
    case DashboardScreen::Sport:
      return "sport";
  }

  return "dashboard";
}

}  // namespace app