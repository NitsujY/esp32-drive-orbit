#pragma once

#include <Arduino.h>

#include "car_telemetry.h"

namespace app {

class WeatherClient {
 public:
  void begin(Print &log);
  void poll(uint32_t now_ms, bool wifi_connected, telemetry::CarTelemetry &telemetry);
  bool hasFreshWeather() const;

 private:
  bool fetchWeather(int8_t *temp_c_out, uint8_t *weather_code_out);

  Print *log_ = nullptr;
  uint32_t connected_since_ms_ = 0;
  uint32_t last_fetch_ms_ = 0;
  bool first_fetch_pending_ = false;
  bool last_wifi_connected_ = false;
  int8_t last_temp_c_ = telemetry::kWeatherTempUnknown;
  uint8_t last_weather_code_ = telemetry::kWeatherCodeUnknown;
  bool has_fresh_weather_ = false;
};

}  // namespace app