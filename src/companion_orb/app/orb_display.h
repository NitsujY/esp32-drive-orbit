#pragma once

#include <Arduino.h>
#include "shared_data.h"
#include "pet_state.h"

namespace orb {

class OrbDisplay {
 public:
  void begin(Print &log);
  void render(const PetState &pet, const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms);
  void renderNoSignal(uint32_t now_ms);
  void renderDebugScreen(const char* status, int16_t speed, int16_t rpm, uint8_t fuel, uint32_t packets, uint32_t now_ms);

 private:
  void drawDataGauge(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void updateBacklight(uint8_t drive_mode, uint32_t now_ms);

  Print *log_ = nullptr;
  bool initialized_ = false;

  int16_t last_spd_ = -1;
  int16_t last_rpm_ = -1;
  float last_gas_ = -1.0f;
  float last_lod_ = -1.0f;
  float last_trp_ = -1.0f;
  int16_t last_rng_ = -1;

  float smooth_gas_ = 0.0f;
  float smooth_lod_ = 0.0f;

  uint8_t backlight_level_ = 255;
  uint32_t last_backlight_ms_ = 0;

  bool in_no_signal_ = false;
  bool in_debug_screen_ = false;
  uint32_t last_no_signal_render_ms_ = 0;
  uint32_t last_render_ms_ = 0;
};

}  // namespace orb
