#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace orb {

class OrbDisplay {
 public:
  void begin(Print &log);
  void render(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms);

 private:
  void drawFullScene(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms);
  void drawMoodRing(uint8_t drive_mode);
  void drawFace(uint8_t drive_mode, uint8_t mood, uint32_t now_ms);
  void drawCoachingScore(uint8_t score, uint8_t drive_mode);
  void drawGearRpm(telemetry::TransmissionGear gear, int16_t rpm, uint8_t drive_mode);

  Print *log_ = nullptr;
  bool initialized_ = false;
  bool needs_full_redraw_ = true;
  uint8_t last_drive_mode_ = 0xFF;
  uint8_t last_mood_ = 0xFF;
  uint8_t last_gear_ = 0xFF;
  int16_t last_rpm_ = -1;
  uint8_t last_score_ = 0xFF;
  uint8_t coaching_score_ = 80;
  int16_t prev_speed_ = 0;
  uint32_t last_score_update_ms_ = 0;
};

}  // namespace orb
