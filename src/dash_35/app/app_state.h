#pragma once

#include <Arduino.h>

#include "dashboard_state.h"
#include "shared_data.h"

namespace app {

struct AppState {
  telemetry::DashboardTelemetry telemetry;
  DashboardViewState view_state;
  bool psram_detected;
  size_t psram_size_bytes;
  bool speed_increasing;
  int16_t previous_speed_kph;
};

AppState makeInitialState();
void advanceSimulation(AppState &state, uint32_t now_ms);

}  // namespace app