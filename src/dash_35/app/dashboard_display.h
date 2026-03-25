#pragma once

#include <Arduino.h>

#include "dashboard_state.h"
#include "shared_data.h"

namespace app {

class DashboardDisplay {
 public:
  void begin(Stream &log);
  void render(const telemetry::DashboardTelemetry &telemetry,
              const DashboardViewState &view_state,
              bool psram_detected,
              uint32_t psram_size_bytes,
              uint32_t now_ms);

 private:
  bool initialized_ = false;
  bool scene_drawn_ = false;
  bool telemetry_cached_ = false;
  uint32_t last_render_ms_ = 0;
  float displayed_rpm_ = -1.0f;
  int16_t last_arc_rpm_drawn_ = -1;
  int16_t last_speed_kph_ = -1;
  int16_t last_rpm_ = -1;
  uint8_t last_fuel_level_pct_ = 255;
  uint16_t last_range_km_ = 0xFFFF;
  telemetry::TransmissionGear last_gear_ = telemetry::TransmissionGear::First;
  DashboardScreen last_screen_ = DashboardScreen::Boot;
  telemetry::DriveMode last_mode_ = telemetry::DriveMode::Cruise;
  bool last_welcome_visible_ = false;
};

}  // namespace app