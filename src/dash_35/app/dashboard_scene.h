#pragma once

#include <Arduino.h>

#include "dashboard_state.h"

namespace app {

enum class DashboardTone : uint8_t {
  Line = 0,
  Muted = 1,
  Text = 2,
  Accent = 3,
  Strong = 4,
  Warning = 5,
  Danger = 6,
};

struct DashboardChipState {
  bool visible;
  DashboardTone text_tone;
  DashboardTone border_tone;
  char text[16];
};

struct DashboardDetailRow {
  char label[24];
  char value[32];
};

constexpr size_t kDashboardDetailRowCount = 8;

struct DashboardSnapshot {
  telemetry::DriveMode mode;
  DashboardScreen screen;
  bool detail_view;
  bool welcome_visible;
  bool time_synced;
  int16_t speed_kph;
  int16_t displayed_rpm;
  uint8_t fuel_level_pct;
  uint16_t estimated_range_km;
  uint8_t range_fill_pct;
  uint8_t fuel_fill_pct;
  uint8_t detail_row_count;
  char vehicle_name[32];
  char subtitle[24];
  char speed_text[8];
  char rpm_text[8];
  char gear_text[8];
  char range_text[16];
  char fuel_text[8];
  char clock_text[16];
  DashboardChipState status_chip;
  DashboardChipState aux_chip;
  DashboardChipState mode_chip;
  DashboardChipState clock_chip;
  DashboardDetailRow detail_rows[kDashboardDetailRowCount];
};

void buildDashboardClockLabel(int hour,
                              int minute,
                              bool synced,
                              char *time_buffer,
                              size_t time_buffer_size);
float smoothDisplayedRpm(float previous_displayed_rpm,
                         int16_t target_rpm,
                         bool reset,
                         uint32_t delta_ms);
DashboardSnapshot buildDashboardSnapshot(const telemetry::DashboardTelemetry &telemetry,
                                         const DashboardViewState &view_state,
                                         bool detail_view,
                                         const char *clock_label,
                                         bool time_synced,
                                         int16_t displayed_rpm);
uint16_t colorForTone(DashboardTone tone, telemetry::DriveMode mode);

}  // namespace app