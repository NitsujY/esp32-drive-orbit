#pragma once

#include <Arduino.h>

#include "car_telemetry.h"

namespace app {

class GatewayStatusDisplay {
 public:
  void begin(Stream &log_output);
  void render(const telemetry::CarTelemetry &telemetry,
              float trip_distance_km,
              const char *access_label,
              bool hosted_ui_ready,
              bool using_embedded_ui,
              bool simulation_enabled,
              uint32_t now_ms);

 private:
  bool initialized_ = false;
  uint32_t last_render_ms_ = 0;
};

}  // namespace app