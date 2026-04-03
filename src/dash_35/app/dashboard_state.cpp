#include "dashboard_state.h"

#include <stdio.h>

#include "trip_analytics.h"
#include "vehicle_profiles/vehicle_profile.h"

namespace app {

namespace {

constexpr uint32_t kBootDurationMs = 1500;
constexpr uint32_t kWelcomeDurationMs = 5000;

void updateTripMetrics(TripMetrics &trip,
                       const telemetry::DashboardTelemetry &telemetry,
                       int16_t previous_speed_kph) {
  const int16_t speed_delta = telemetry.speed_kph - previous_speed_kph;
  const float maf_gps = trip_analytics::estimateMafGps(telemetry.rpm, telemetry.speed_kph,
                                                       telemetry.longitudinal_accel_mg);
  const trip_analytics::FuelConsumptionMetrics fuel_metrics =
      trip_analytics::calculateFuelConsumption(maf_gps, telemetry.speed_kph);
  const float target_l_per_100km =
      100.0f / vehicle_profiles::activeProfile().baseline_efficiency_km_per_l;

  trip.estimated_maf_gps = maf_gps;
  trip.instant_l_per_100km = fuel_metrics.liters_per_100km;
  trip.instant_gph = fuel_metrics.gallons_per_hour;

  if ((speed_delta >= 5 || telemetry.longitudinal_accel_mg >= 220) &&
      trip.harsh_acceleration_count < 255) {
    ++trip.harsh_acceleration_count;
  }

  if ((speed_delta <= -5 || telemetry.longitudinal_accel_mg <= -220) &&
      trip.harsh_braking_count < 255) {
    ++trip.harsh_braking_count;
  }

  if (trip.last_sample_ms != 0 && telemetry.uptime_ms > trip.last_sample_ms) {
    const uint32_t dt_ms = telemetry.uptime_ms - trip.last_sample_ms;
    if (dt_ms >= 50U && dt_ms <= 2000U) {
      trip.fuel_used_l += fuel_metrics.liters_per_hour * (static_cast<float>(dt_ms) / 3600000.0f);
    }
  }
  trip.last_sample_ms = telemetry.uptime_ms;

  if (trip.trip_distance_km >= 0.05f && trip.fuel_used_l > 0.0f) {
    trip.avg_l_per_100km = (trip.fuel_used_l / trip.trip_distance_km) * 100.0f;
  } else {
    trip.avg_l_per_100km = fuel_metrics.liters_per_100km;
  }

  trip.fuel_saving_score =
      trip_analytics::scoreFuelSaving(trip.avg_l_per_100km, target_l_per_100km,
                                      trip.harsh_acceleration_count);
  trip.comfort_score = trip_analytics::scoreComfort(telemetry.longitudinal_accel_mg,
                                                    trip.harsh_acceleration_count,
                                                    trip.harsh_braking_count);
  trip.flow_score = trip_analytics::scoreFlow(speed_delta, telemetry.speed_kph);
  trip.trip_score = trip_analytics::scoreTripOverall(
      trip.fuel_saving_score, trip.comfort_score, trip.flow_score);

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

  if (trip.fuel_saving_score < 62) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Fuel burn is trending high. Short-shift sooner and ease throttle ramps.");
    return;
  }

  if (trip.comfort_score < 64) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Ride is getting busy. Smooth braking release and trim throttle spikes.");
    return;
  }

  if (telemetry.drive_mode == telemetry::DriveMode::Sport) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Sport pace is active. Keep exits clean so the trip score stays balanced.");
    return;
  }

  if (trip.trip_score >= 84) {
    snprintf(trip.coaching_message, sizeof(trip.coaching_message),
             "Trip looks tidy. Keep this rhythm for efficient and comfortable progress.");
    return;
  }

  snprintf(trip.coaching_message, sizeof(trip.coaching_message),
           "Trip settled in. Clean inputs now will lift both comfort and economy.");
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
  view_state.trip.fuel_used_l = 0.0f;
  view_state.trip.avg_l_per_100km = 0.0f;
  view_state.trip.instant_l_per_100km = 0.0f;
  view_state.trip.instant_gph = 0.0f;
  view_state.trip.estimated_maf_gps = 0.0f;
  view_state.trip.harsh_acceleration_count = 0;
  view_state.trip.harsh_braking_count = 0;
  view_state.trip.fuel_saving_score = 100;
  view_state.trip.comfort_score = 100;
  view_state.trip.flow_score = 100;
  view_state.trip.trip_score = 100;
  view_state.trip.last_sample_ms = 0;
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