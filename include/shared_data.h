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

enum class TransmissionGear : uint8_t {
  Park = 0,
  First = 1,
  Second = 2,
  Third = 3,
  Fourth = 4,
  Fifth = 5,
  Sixth = 6,
};

struct DashboardTelemetry {
  uint32_t sequence;
  uint32_t uptime_ms;
  int16_t rpm;
  int16_t speed_kph;
  int16_t coolant_temp_c;
  uint16_t battery_mv;
  uint8_t fuel_level_pct;
  uint16_t estimated_range_km;
  DriveMode drive_mode;
  CompanionMood companion_mood;
  TransmissionGear gear;
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

inline TransmissionGear resolveGear(int16_t speed_kph, int16_t rpm) {
  if (rpm <= 0) {
    return TransmissionGear::Park;
  }

  if (speed_kph < 12) {
    return TransmissionGear::First;
  }

  if (speed_kph < 28) {
    return TransmissionGear::Second;
  }

  if (speed_kph < 44) {
    return TransmissionGear::Third;
  }

  if (speed_kph < 62) {
    return TransmissionGear::Fourth;
  }

  if (speed_kph < 82) {
    return TransmissionGear::Fifth;
  }

  return TransmissionGear::Sixth;
}

inline uint16_t estimateRangeKm(uint8_t fuel_level_pct, int16_t speed_kph) {
  const float efficiency_factor = constrain(1.02f - (speed_kph / 260.0f), 0.72f, 1.02f);
  return static_cast<uint16_t>(fuel_level_pct * 4.6f * efficiency_factor);
}

inline void refreshDerivedTelemetry(DashboardTelemetry &telemetry) {
  telemetry.estimated_range_km = estimateRangeKm(telemetry.fuel_level_pct, telemetry.speed_kph);
  telemetry.drive_mode = resolveDriveMode(telemetry.speed_kph);
  telemetry.companion_mood = resolveCompanionMood(telemetry.speed_kph);
  telemetry.gear = resolveGear(telemetry.speed_kph, telemetry.rpm);
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
  refreshDerivedTelemetry(telemetry);
  return telemetry;
}

}  // namespace telemetry