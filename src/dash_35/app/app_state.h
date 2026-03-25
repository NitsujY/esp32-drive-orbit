#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace app {

struct AppState {
  telemetry::DashboardTelemetry telemetry;
  bool psram_detected;
  size_t psram_size_bytes;
  bool speed_increasing;
};

AppState makeInitialState();
void advanceSimulation(AppState &state, uint32_t now_ms);

}  // namespace app