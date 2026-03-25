#pragma once

#include <Arduino.h>

namespace telemetry {

enum class DriveMode : uint8_t {
  Calm = 0,
  Cruise = 1,
  Sport = 2,
};

enum class CompanionMood : uint8_t {
  Idle = 0,
  Warm = 1,
  Alert = 2,
};

struct DashboardTelemetry {
  uint32_t sequence;
  uint32_t uptime_ms;
  int16_t rpm;
  int16_t speed_kph;
  int16_t coolant_temp_c;
  uint16_t battery_mv;
  uint8_t fuel_level_pct;
  DriveMode drive_mode;
  CompanionMood companion_mood;
};

inline DriveMode resolveDriveMode(int16_t speed_kph) {
  return speed_kph > 80 ? DriveMode::Sport : DriveMode::Cruise;
}

inline CompanionMood resolveCompanionMood(int16_t speed_kph) {
  if (speed_kph < 10) {
    return CompanionMood::Idle;
  }

  if (speed_kph < 45) {
    return CompanionMood::Warm;
  }

  return CompanionMood::Alert;
}

inline DashboardTelemetry makeSimulatedTelemetry(uint32_t sequence,
                                                 uint32_t uptime_ms,
                                                 int16_t rpm,
                                                 int16_t speed_kph,
                                                 int16_t coolant_temp_c,
                                                 uint16_t battery_mv,
                                                 uint8_t fuel_level_pct) {
  DashboardTelemetry telemetry{};
  telemetry.sequence = sequence;
  telemetry.uptime_ms = uptime_ms;
  telemetry.rpm = rpm;
  telemetry.speed_kph = speed_kph;
  telemetry.coolant_temp_c = coolant_temp_c;
  telemetry.battery_mv = battery_mv;
  telemetry.fuel_level_pct = fuel_level_pct;
  telemetry.drive_mode = resolveDriveMode(speed_kph);
  telemetry.companion_mood = resolveCompanionMood(speed_kph);
  return telemetry;
}

}  // namespace telemetry