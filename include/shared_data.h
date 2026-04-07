#pragma once

#include <Arduino.h>

namespace telemetry {

#ifndef TELEMETRY_WEATHER_CONSTANTS_DEFINED
#define TELEMETRY_WEATHER_CONSTANTS_DEFINED
constexpr int8_t kWeatherTempUnknown = -128;
constexpr uint8_t kWeatherCodeUnknown = 255;
#endif
constexpr uint16_t kNearestCameraUnknown = 0xFFFF;

enum class DriveMode : uint8_t {
  Calm = 0,
  Cruise = 1,
  Sport = 2,
};

enum class CompanionMood : uint8_t {
  Idle = 0,
  Warm = 1,
  Alert = 2,
  Happy = 3,
  Sad = 4,
  Excited = 5,
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
  int16_t longitudinal_accel_mg;
  int16_t coolant_temp_c;
  uint16_t battery_mv;
  uint8_t fuel_level_pct;
  uint16_t estimated_range_km;
  uint16_t nearest_camera_m;
  DriveMode drive_mode;
  CompanionMood companion_mood;
  TransmissionGear gear;
  int8_t weather_temp_c;
  uint8_t weather_code;
  bool wifi_connected;
  bool headlights_on;
};

inline DriveMode resolveDriveMode(int16_t speed_kph) {
  return speed_kph > 80 ? DriveMode::Sport : DriveMode::Cruise;
}

inline CompanionMood resolveCompanionMood(int16_t speed_kph, int16_t rpm = 0) {
  // Excited: high RPM burst (sport driving).
  if (rpm > 4500) {
    return CompanionMood::Excited;
  }

  // Alert: fast driving.
  if (speed_kph > 80) {
    return CompanionMood::Alert;
  }

  // Happy: steady cruising at moderate speed.
  if (speed_kph >= 30 && speed_kph <= 80) {
    return CompanionMood::Happy;
  }

  // Warm: gentle city driving.
  if (speed_kph >= 10) {
    return CompanionMood::Warm;
  }

  // Sad: decelerating to stop or very slow.
  if (speed_kph > 0 && speed_kph < 10 && rpm < 900) {
    return CompanionMood::Sad;
  }

  // Idle: parked.
  return CompanionMood::Idle;
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
  telemetry.companion_mood = resolveCompanionMood(telemetry.speed_kph, telemetry.rpm);
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
  telemetry.longitudinal_accel_mg = 0;
  telemetry.coolant_temp_c = coolant_temp_c;
  telemetry.battery_mv = battery_mv;
  telemetry.fuel_level_pct = fuel_level_pct;
  telemetry.weather_temp_c = kWeatherTempUnknown;
  telemetry.weather_code = kWeatherCodeUnknown;
  telemetry.wifi_connected = false;
  telemetry.nearest_camera_m = kNearestCameraUnknown;
  refreshDerivedTelemetry(telemetry);
  return telemetry;
}

}  // namespace telemetry