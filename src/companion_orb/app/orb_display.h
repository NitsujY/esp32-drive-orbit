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
  void drawCameraBadge(uint16_t nearest_m);
  void drawMoodRing(uint8_t drive_mode, int16_t rpm, uint32_t now_ms);
  void drawFace(uint8_t drive_mode, uint8_t mood, int16_t rpm, uint32_t now_ms);
  void drawLowFuelFace(uint8_t drive_mode, uint32_t now_ms);
  void drawSleepFace(uint8_t drive_mode);
  void drawCoachingScore(uint8_t score, int16_t longitudinal_accel_mg, uint8_t drive_mode);
  void drawGearRpm(telemetry::TransmissionGear gear,
                   int16_t rpm,
                   int16_t longitudinal_accel_mg,
                   uint8_t drive_mode);
  void updateBacklight(uint8_t drive_mode, int16_t speed_kph, uint32_t now_ms);
  float modeTransition(uint8_t target_dm, uint32_t now_ms);

  Print *log_ = nullptr;
  bool initialized_ = false;
  bool needs_full_redraw_ = true;

  // Cached state for incremental updates.
  uint8_t last_drive_mode_ = 0xFF;
  uint8_t last_mood_ = 0xFF;
  uint8_t last_gear_ = 0xFF;
  int16_t last_rpm_ = -1;
  int16_t last_accel_mg_ = 0;
  uint8_t last_score_ = 0xFF;
  uint8_t last_fuel_pct_ = 0xFF;
  bool last_low_fuel_ = false;
  uint16_t last_nearest_camera_m_ = 0xFFFF;

  // Coaching score.
  uint8_t coaching_score_ = 80;
  int16_t prev_speed_ = 0;
  uint32_t last_score_update_ms_ = 0;

  // Mode transition (smooth fade).
  float mode_blend_ = 0.0f;       // 0.0 = chill, 1.0 = sport
  uint8_t target_drive_mode_ = 0;
  uint32_t mode_transition_start_ms_ = 0;

  // Breathing animation.
  float breath_phase_ = 0.0f;

  // Sleep detection.
  uint32_t last_motion_ms_ = 0;
  bool sleeping_ = false;

  // Backlight.
  uint8_t backlight_level_ = 255;
  uint32_t last_backlight_ms_ = 0;
};

}  // namespace orb
