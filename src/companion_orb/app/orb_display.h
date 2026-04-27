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
  void renderDebugScreen(const char* status, const telemetry::DashboardTelemetry &t, uint32_t now_ms);

 private:
  void drawDataGauge(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void renderParkedScreen(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void updateBacklight(uint8_t drive_mode, uint32_t now_ms);

  Print *log_ = nullptr;
  bool initialized_ = false;

  int16_t last_spd_ = -1;
  bool last_night_mode_ = false;
  int16_t last_rpm_ = -1;
  float last_gas_ = -1.0f;
  float last_lod_ = -1.0f;
  float last_trp_ = -1.0f;
  int16_t last_rng_ = -1;

  float smooth_gas_ = 0.0f;
  float smooth_lod_ = 0.0f;

  uint8_t backlight_level_ = 255;
  uint32_t last_backlight_ms_ = 0;

  bool in_no_signal_    = false;
  bool in_debug_screen_ = false;
  uint32_t last_no_signal_render_ms_ = 0;
  uint32_t last_render_ms_           = 0;
  uint32_t last_debug_render_ms_     = 0;  // separate clock so debug never blocks render()

  // ── Parked / WiFi info screen ──────────────────────────────────────────────
  // Speed must be 0 for this many ms before we switch to the parked screen.
  static constexpr uint32_t kParkDelayMs = 2000;
  // Parked screen refreshes at 1 Hz (clock tick + weather update).
  static constexpr uint32_t kParkedRefreshMs = 1000;

  uint32_t stopped_since_ms_    = 0;   // millis() when speed first hit 0
  bool     in_parked_screen_    = false;
  bool     greeting_shown_      = false;
  uint32_t greeting_start_ms_   = 0;
  uint32_t last_parked_render_ms_ = 0;

  // Cached values so we only redraw when something changes.
  int8_t   last_park_temp_ = telemetry::kWeatherTempUnknown;
  uint8_t  last_park_hh_   = 0xFF;   // last drawn hour
  uint8_t  last_park_mm_   = 0xFF;   // last drawn minute
};

}  // namespace orb
