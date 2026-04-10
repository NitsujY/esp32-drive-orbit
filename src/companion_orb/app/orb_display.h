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

 private:
  void drawFullScene(const PetState &pet, const telemetry::DashboardTelemetry &t, uint32_t now_ms);

  // XP ring replaces the old mood ring.
  void drawXpRing(const PetState &pet, uint8_t drive_mode, int16_t rpm, uint32_t now_ms);

  // The orb IS the face — eyes/mouth fill the whole 240×240 display interior.
  void drawOrbFace(uint8_t mood, const DrivingDynamics &dyn, uint32_t now_ms);
  void drawOrbFaceSleeping(uint32_t now_ms);

  // Reaction overlays (hearts, stars, startled flash, etc.).
  void drawReaction(Reaction reaction, uint32_t age_ms, uint32_t now_ms);

  // Stat bars at the bottom.
  void drawStatBars(uint8_t happiness, uint8_t energy, PetStage stage);

  // Backlight.
  void updateBacklight(uint8_t drive_mode, const PetState &pet, uint32_t now_ms);

  // Mode transition.
  float modeTransition(uint8_t target_dm, uint32_t now_ms);

  Print *log_ = nullptr;
  bool initialized_ = false;
  bool needs_full_redraw_ = true;

  // Cached state.
  uint8_t last_drive_mode_ = 0xFF;
  uint8_t last_mood_ = 0xFF;
  int16_t last_rpm_ = -1;
  PetStage last_stage_ = PetStage::Egg;

  // Mode transition.
  float mode_blend_ = 0.0f;
  uint8_t target_drive_mode_ = 0;
  uint32_t mode_transition_start_ms_ = 0;

  // Breathing animation.
  float breath_phase_ = 0.0f;

  // Backlight.
  uint8_t backlight_level_ = 255;
  uint32_t last_backlight_ms_ = 0;

  // No-signal.
  bool in_no_signal_ = false;
  uint32_t last_no_signal_render_ms_ = 0;
  // Rendering throttle to avoid excessive partial updates (ms).
  uint32_t last_render_ms_ = 0;
};

}  // namespace orb
