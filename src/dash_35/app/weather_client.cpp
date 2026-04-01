#include "weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "wifi_config.h"

namespace app {

namespace {

constexpr uint32_t kFirstFetchDelayMs = 5000;
constexpr uint32_t kFetchIntervalMs = 600000;

String buildWeatherUrl() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(LOCATION_LAT, 4);
  url += "&longitude=";
  url += String(LOCATION_LON, 4);
  url += "&current_weather=true";
  return url;
}

}  // namespace

void WeatherClient::begin(Print &log) {
  log_ = &log;
}

void WeatherClient::poll(uint32_t now_ms,
                         bool wifi_connected,
                         telemetry::DashboardTelemetry &telemetry) {
  telemetry.wifi_connected = wifi_connected;

  if (!wifi_connected) {
    last_wifi_connected_ = false;
    connected_since_ms_ = 0;
    first_fetch_pending_ = false;
    telemetry.weather_temp_c = last_temp_c_;
    telemetry.weather_code = last_weather_code_;
    return;
  }

  if (!last_wifi_connected_) {
    connected_since_ms_ = now_ms;
    first_fetch_pending_ = true;
    if (log_ != nullptr) {
      log_->println("[WEATHER] Wi-Fi up, scheduling fetch");
    }
  }
  last_wifi_connected_ = true;

  const bool first_fetch_due = first_fetch_pending_ &&
                               connected_since_ms_ > 0 &&
                               (now_ms - connected_since_ms_ >= kFirstFetchDelayMs);
  const bool periodic_fetch_due = last_fetch_ms_ > 0 &&
                                  (now_ms - last_fetch_ms_ >= kFetchIntervalMs);

  if (first_fetch_due || periodic_fetch_due || last_fetch_ms_ == 0) {
    int8_t temp_c = last_temp_c_;
    uint8_t weather_code = last_weather_code_;
    if (fetchWeather(&temp_c, &weather_code)) {
      last_temp_c_ = temp_c;
      last_weather_code_ = weather_code;
      last_fetch_ms_ = now_ms;
      first_fetch_pending_ = false;
    } else if (last_fetch_ms_ == 0) {
      last_fetch_ms_ = now_ms;
    }
  }

  telemetry.weather_temp_c = last_temp_c_;
  telemetry.weather_code = last_weather_code_;
}

bool WeatherClient::fetchWeather(int8_t *temp_c_out, uint8_t *weather_code_out) {
  WiFiClientSecure secure_client;
  secure_client.setInsecure();

  HTTPClient http;
  const String url = buildWeatherUrl();
  if (!http.begin(secure_client, url)) {
    if (log_ != nullptr) {
      log_->println("[WEATHER] HTTP begin failed");
    }
    return false;
  }

  http.setTimeout(5000);
  const int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    if (log_ != nullptr) {
      log_->print("[WEATHER] HTTP GET failed: ");
      log_->println(http_code);
    }
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error) {
    if (log_ != nullptr) {
      log_->print("[WEATHER] JSON parse failed: ");
      log_->println(error.c_str());
    }
    return false;
  }

  JsonVariant current_weather = doc["current_weather"];
  if (current_weather.isNull()) {
    if (log_ != nullptr) {
      log_->println("[WEATHER] Missing current_weather in response");
    }
    return false;
  }

  const float temp_c = current_weather["temperature"] | NAN;
  const int weather_code = current_weather["weathercode"] | -1;
  if (isnan(temp_c) || weather_code < 0 || weather_code > 255) {
    if (log_ != nullptr) {
      log_->println("[WEATHER] Incomplete weather payload");
    }
    return false;
  }

  if (temp_c_out != nullptr) {
    *temp_c_out = static_cast<int8_t>(lroundf(temp_c));
  }
  if (weather_code_out != nullptr) {
    *weather_code_out = static_cast<uint8_t>(weather_code);
  }

  if (log_ != nullptr) {
    log_->print("[WEATHER] ");
    log_->print(static_cast<int>(lroundf(temp_c)));
    log_->print("C code=");
    log_->println(weather_code);
  }

  return true;
}

}  // namespace app