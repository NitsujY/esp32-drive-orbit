#pragma once

#include <Arduino.h>

#include "car_telemetry.h"

namespace app {

class SimulationController {
 public:
  void begin(Print &log_output);
  void poll(uint32_t now_ms);
  bool enabled() const;
  bool apply(telemetry::CarTelemetry &telemetry, uint32_t now_ms, bool wifi_connected);

 private:
  void resetState();
  static float easeInOutSine(float value);
  static float sampleSpeed(float phase_seconds);

  Print *log_output_ = nullptr;
  bool enabled_ = false;
  bool button_was_down_ = false;
  uint32_t last_button_sample_ms_ = 0;
  uint32_t last_toggle_ms_ = 0;
  uint32_t sequence_ = 0;
  float phase_s_ = 0.0f;
  uint32_t uptime_ms_ = 0;
  uint32_t last_tick_ms_ = 0;
  int16_t last_speed_kph_ = 0;
  telemetry::CarTelemetry snapshot_ = telemetry::makeInitialCarTelemetry();
};

}  // namespace app