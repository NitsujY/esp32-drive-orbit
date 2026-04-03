#include "dashboard_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cyber_theme.h"
#include "vehicle_profiles/vehicle_profile.h"

namespace app {

namespace {

constexpr uint16_t kMaxRangeKm = 360;

const char *modeChipLabel(telemetry::DriveMode mode) {
  switch (mode) {
    case telemetry::DriveMode::Calm:
      return "CALM";
    case telemetry::DriveMode::Cruise:
      return "CRUISE";
    case telemetry::DriveMode::Sport:
      return "SPORT";
  }

  return "CRUISE";
}

const char *gearLabel(telemetry::TransmissionGear gear) {
  switch (gear) {
    case telemetry::TransmissionGear::Park:
      return "P";
    case telemetry::TransmissionGear::First:
      return "D1";
    case telemetry::TransmissionGear::Second:
      return "D2";
    case telemetry::TransmissionGear::Third:
      return "D3";
    case telemetry::TransmissionGear::Fourth:
      return "D4";
    case telemetry::TransmissionGear::Fifth:
      return "D5";
    case telemetry::TransmissionGear::Sixth:
      return "D6";
  }

  return "P";
}

const char *compactStatusDetail(ObdConnectionState state, bool welcome_visible) {
  if (state == ObdConnectionState::Live) {
    return welcome_visible ? "IGNITION" : "LIVE";
  }

  if (state == ObdConnectionState::Connecting) {
    return "SCANNING";
  }

  return "OBD OFF";
}

const char *screenTitle(DashboardScreen screen) {
  switch (screen) {
    case DashboardScreen::Boot:
      return "Boot";
    case DashboardScreen::Welcome:
      return "Welcome";
    case DashboardScreen::Dashboard:
      return "Drive";
    case DashboardScreen::Trip:
      return "Trip";
    case DashboardScreen::Health:
      return "Health";
    case DashboardScreen::Sport:
      return "Sport";
  }

  return "Drive";
}

bool shouldShowCameraChip(uint16_t nearest_camera_m) {
  return nearest_camera_m != telemetry::kNearestCameraUnknown && nearest_camera_m <= 1000;
}

void buildCameraChipLabel(char *buffer, size_t buffer_size, uint16_t nearest_camera_m) {
  if (!shouldShowCameraChip(nearest_camera_m)) {
    buffer[0] = '\0';
    return;
  }

  if (nearest_camera_m >= 1000) {
    snprintf(buffer, buffer_size, "CAM 1km");
    return;
  }

  snprintf(buffer, buffer_size, "CAM %um", nearest_camera_m);
}

void setChip(DashboardChipState &chip,
             const char *text,
             DashboardTone text_tone,
             DashboardTone border_tone,
             bool visible = true) {
  chip.visible = visible;
  chip.text_tone = text_tone;
  chip.border_tone = border_tone;
  snprintf(chip.text, sizeof(chip.text), "%s", text != nullptr ? text : "");
}

}  // namespace

void buildDashboardClockLabel(int hour,
                              int minute,
                              bool synced,
                              char *time_buffer,
                              size_t time_buffer_size) {
  if (!synced) {
    snprintf(time_buffer, time_buffer_size, "--:--");
    return;
  }

  snprintf(time_buffer, time_buffer_size, "%02d:%02d", hour, minute);
}

float smoothDisplayedRpm(float previous_displayed_rpm,
                         int16_t target_rpm,
                         bool reset,
                         uint32_t delta_ms) {
  if (reset || previous_displayed_rpm < 0.0f) {
    return static_cast<float>(target_rpm);
  }

  float displayed_rpm = previous_displayed_rpm;
  const float smoothing = min(1.0f, 0.22f * (static_cast<float>(delta_ms) / 33.0f));
  displayed_rpm += (static_cast<float>(target_rpm) - displayed_rpm) * smoothing;
  if (fabsf(displayed_rpm - static_cast<float>(target_rpm)) < 1.0f) {
    displayed_rpm = static_cast<float>(target_rpm);
  }
  return displayed_rpm;
}

DashboardSnapshot buildDashboardSnapshot(const telemetry::DashboardTelemetry &telemetry,
                                         const DashboardViewState &view_state,
                                         bool detail_view,
                                         const char *clock_label,
                                         bool time_synced,
                                         int16_t displayed_rpm) {
  DashboardSnapshot snapshot{};
  snapshot.mode = telemetry.drive_mode;
  snapshot.screen = view_state.active_screen;
  snapshot.detail_view = detail_view;
  snapshot.welcome_visible = view_state.welcome_visible;
  snapshot.time_synced = time_synced;
  snapshot.speed_kph = telemetry.speed_kph;
  snapshot.displayed_rpm = displayed_rpm;
  snapshot.fuel_level_pct = telemetry.fuel_level_pct;
  snapshot.estimated_range_km = telemetry.estimated_range_km;
  snapshot.range_fill_pct = static_cast<uint8_t>(constrain(
      static_cast<long>(telemetry.estimated_range_km) * 100L / kMaxRangeKm, 0L, 100L));
  snapshot.fuel_fill_pct = telemetry.fuel_level_pct;
  snapshot.detail_row_count = 0;

  snprintf(snapshot.vehicle_name, sizeof(snapshot.vehicle_name), "%s",
           vehicle_profiles::activeProfile().display_name);
  snprintf(snapshot.subtitle, sizeof(snapshot.subtitle), "%s",
           detail_view ? "Trip detail"
                       : screenTitle(view_state.active_screen));
  snprintf(snapshot.speed_text, sizeof(snapshot.speed_text), "%d", telemetry.speed_kph);
  snprintf(snapshot.rpm_text, sizeof(snapshot.rpm_text), "%d", displayed_rpm);
  snprintf(snapshot.gear_text, sizeof(snapshot.gear_text), "%s", gearLabel(telemetry.gear));
  snprintf(snapshot.range_text, sizeof(snapshot.range_text), "%u km",
           telemetry.estimated_range_km);
  snprintf(snapshot.fuel_text, sizeof(snapshot.fuel_text), "%u%%", telemetry.fuel_level_pct);
  snprintf(snapshot.clock_text, sizeof(snapshot.clock_text), "%s",
           clock_label != nullptr ? clock_label : "--:--");

  setChip(snapshot.status_chip, compactStatusDetail(view_state.obd_connection_state,
                                                    view_state.welcome_visible),
          view_state.obd_connection_state == ObdConnectionState::Disconnected
              ? DashboardTone::Muted
              : (view_state.obd_connection_state == ObdConnectionState::Connecting
                     ? DashboardTone::Warning
                     : DashboardTone::Accent),
          DashboardTone::Line);

  if (shouldShowCameraChip(telemetry.nearest_camera_m)) {
    char camera_chip[16];
    buildCameraChipLabel(camera_chip, sizeof(camera_chip), telemetry.nearest_camera_m);
    setChip(snapshot.aux_chip, camera_chip,
            telemetry.nearest_camera_m < 200 ? DashboardTone::Danger : DashboardTone::Warning,
            telemetry.nearest_camera_m < 200 ? DashboardTone::Danger : DashboardTone::Warning);
  } else if (detail_view) {
    setChip(snapshot.aux_chip, telemetry.wifi_connected ? "WIFI ON" : "WIFI OFF",
            telemetry.wifi_connected ? DashboardTone::Strong : DashboardTone::Muted,
            telemetry.wifi_connected ? DashboardTone::Strong : DashboardTone::Line);
  } else {
    setChip(snapshot.aux_chip, "", DashboardTone::Muted, DashboardTone::Line, false);
  }

  setChip(snapshot.mode_chip, modeChipLabel(telemetry.drive_mode), DashboardTone::Strong,
          DashboardTone::Line);
  setChip(snapshot.clock_chip, snapshot.clock_text,
          time_synced ? DashboardTone::Strong : DashboardTone::Muted, DashboardTone::Line);

  auto addRow = [&](const char *label, const char *value) {
    if (snapshot.detail_row_count >= kDashboardDetailRowCount) {
      return;
    }
    DashboardDetailRow &row = snapshot.detail_rows[snapshot.detail_row_count++];
    snprintf(row.label, sizeof(row.label), "%s", label);
    snprintf(row.value, sizeof(row.value), "%s", value);
  };

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d C", telemetry.coolant_temp_c);
  addRow("Coolant Temp", buffer);
  snprintf(buffer, sizeof(buffer), "%.1f V", telemetry.battery_mv / 1000.0f);
  addRow("Battery", buffer);
  snprintf(buffer, sizeof(buffer), "%u%%", telemetry.fuel_level_pct);
  addRow("Fuel Level", buffer);
  snprintf(buffer, sizeof(buffer), "%u km", telemetry.estimated_range_km);
  addRow("Est. Range", buffer);
  snprintf(buffer, sizeof(buffer), "%.1f km", view_state.trip.trip_distance_km);
  addRow("Trip Distance", buffer);
  if (telemetry.nearest_camera_m == telemetry::kNearestCameraUnknown) {
    snprintf(buffer, sizeof(buffer), "No data");
  } else if (telemetry.nearest_camera_m > 1000) {
    snprintf(buffer, sizeof(buffer), "> 1 km");
  } else {
    snprintf(buffer, sizeof(buffer), "%u m", telemetry.nearest_camera_m);
  }
  addRow("Nearest Cam", buffer);
  addRow("WiFi", telemetry.wifi_connected ? "Connected" : "Offline");
  snprintf(buffer, sizeof(buffer), "%s", snapshot.gear_text);
  addRow("Gear", buffer);

  return snapshot;
}

uint16_t colorForTone(DashboardTone tone, telemetry::DriveMode mode) {
  switch (tone) {
    case DashboardTone::Line:
      return theme::line(static_cast<uint8_t>(mode));
    case DashboardTone::Muted:
      return theme::muted();
    case DashboardTone::Text:
      return theme::text();
    case DashboardTone::Accent:
      return theme::accent(static_cast<uint8_t>(mode));
    case DashboardTone::Strong:
      return theme::accentStrong(static_cast<uint8_t>(mode));
    case DashboardTone::Warning:
      return theme::warning();
    case DashboardTone::Danger:
      return theme::rgb565(255, 84, 84);
  }

  return theme::text();
}

}  // namespace app