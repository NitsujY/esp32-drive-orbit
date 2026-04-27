#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace orb {

class WeatherClient {
 public:
  void begin(Print &log);
  void poll(uint32_t now_ms, bool wifi_connected, telemetry::DashboardTelemetry &telemetry);
  bool hasFreshWeather() const;

 private:
  bool fetchWeather(int8_t *temp_c_out, uint8_t *weather_code_out, uint16_t *sunset_min_out);
  bool fetchGreeting(char *greeting_out, size_t max_len);

  Print *log_ = nullptr;
  uint32_t connected_since_ms_ = 0;
  uint32_t last_fetch_ms_ = 0;
  bool first_fetch_pending_ = false;
  bool last_wifi_connected_ = false;
  int8_t last_temp_c_ = telemetry::kWeatherTempUnknown;
  uint8_t last_weather_code_ = telemetry::kWeatherCodeUnknown;
  uint16_t last_sunset_minute_ = 0xFFFF;
  char last_greeting_[64] = {0};
  bool has_fresh_weather_ = false;
};

}  // namespace orb
