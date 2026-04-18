#pragma once

#include <Arduino.h>

namespace telemetry {

#ifndef TELEMETRY_WEATHER_CONSTANTS_DEFINED
#define TELEMETRY_WEATHER_CONSTANTS_DEFINED
constexpr int8_t kWeatherTempUnknown = -128;
constexpr uint8_t kWeatherCodeUnknown = 255;
#endif

struct CarTelemetry {
  uint32_t sequence;
  uint32_t uptime_ms;
  int16_t rpm;
  int16_t speed_kph;
  int16_t longitudinal_accel_mg;
  int16_t coolant_temp_c;
  int8_t weather_temp_c;
  uint8_t weather_code;
  uint16_t battery_mv;
  uint8_t fuel_level_pct;
  uint16_t estimated_range_km;
  bool wifi_connected;
  bool headlights_on;
  bool obd_connected;
  bool telemetry_fresh;
  uint8_t app_ws_clients;   // number of WebSocket clients currently connected
  bool orb_transmitting;    // true when ESP-NOW transmitter is active
};

inline uint16_t estimateGatewayRangeKm(uint8_t fuel_level_pct, int16_t speed_kph) {
  const float efficiency_factor = constrain(1.02f - (speed_kph / 260.0f), 0.72f, 1.02f);
  return static_cast<uint16_t>(fuel_level_pct * 4.6f * efficiency_factor);
}

inline void refreshDerivedTelemetry(CarTelemetry &telemetry) {
  telemetry.estimated_range_km =
      estimateGatewayRangeKm(telemetry.fuel_level_pct, telemetry.speed_kph);
}

inline CarTelemetry makeInitialCarTelemetry() {
  CarTelemetry telemetry{};
  telemetry.sequence = 0;
  telemetry.uptime_ms = 0;
  telemetry.rpm = 0;
  telemetry.speed_kph = 0;
  telemetry.longitudinal_accel_mg = 0;
  telemetry.coolant_temp_c = 0;
  telemetry.weather_temp_c = kWeatherTempUnknown;
  telemetry.weather_code = kWeatherCodeUnknown;
  telemetry.battery_mv = 0;
  telemetry.fuel_level_pct = 0;
  telemetry.estimated_range_km = 0;
  telemetry.wifi_connected = false;
  telemetry.headlights_on = false;
  telemetry.obd_connected = false;
  telemetry.telemetry_fresh = false;
  telemetry.app_ws_clients = 0;
  telemetry.orb_transmitting = false;
  return telemetry;
}

}  // namespace telemetry