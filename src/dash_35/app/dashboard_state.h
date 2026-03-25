#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace app {

enum class DashboardScreen : uint8_t {
  Boot = 0,
  Welcome = 1,
  Dashboard = 2,
  Trip = 3,
  Health = 4,
  Sport = 5,
};

struct TripMetrics {
  uint8_t harsh_acceleration_count;
  uint8_t harsh_braking_count;
  uint8_t smoothness_score;
  uint8_t speed_consistency_score;
  char coaching_message[96];
};

struct DashboardViewState {
  DashboardScreen active_screen;
  bool welcome_visible;
  bool health_attention;
  TripMetrics trip;
};

DashboardViewState makeInitialDashboardViewState();
void updateDashboardViewState(DashboardViewState &view_state,
                              const telemetry::DashboardTelemetry &telemetry,
                              int16_t previous_speed_kph);
const char *dashboardScreenName(DashboardScreen screen);

}  // namespace app