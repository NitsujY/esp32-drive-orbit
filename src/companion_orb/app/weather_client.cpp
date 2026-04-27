#include "weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "wifi_config.h"

namespace orb {

namespace {

constexpr uint32_t kFirstFetchDelayMs = 5000;
constexpr uint32_t kFetchIntervalMs = 600000;

String buildWeatherUrl() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(LOCATION_LAT, 4);
  url += "&longitude=";
  url += String(LOCATION_LON, 4);
  url += "&current_weather=true&daily=sunset&timezone=auto";
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
    telemetry.sunset_minute = last_sunset_minute_;
    strlcpy(telemetry.greeting, last_greeting_, sizeof(telemetry.greeting));
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
    uint16_t sunset_min = last_sunset_minute_;
    if (fetchWeather(&temp_c, &weather_code, &sunset_min)) {
      last_temp_c_ = temp_c;
      last_weather_code_ = weather_code;
      last_sunset_minute_ = sunset_min;
      last_fetch_ms_ = now_ms;
      first_fetch_pending_ = false;
      if (last_greeting_[0] == '\0') {
        fetchGreeting(last_greeting_, sizeof(last_greeting_));
      }
    } else {
      last_fetch_ms_ = now_ms;
      // Delay the retries by resetting the connected time
      if (first_fetch_pending_) {
        connected_since_ms_ = now_ms;
      }
    }
  }

  telemetry.weather_temp_c = last_temp_c_;
  telemetry.weather_code = last_weather_code_;
  telemetry.sunset_minute = last_sunset_minute_;
  strlcpy(telemetry.greeting, last_greeting_, sizeof(telemetry.greeting));
}

bool WeatherClient::hasFreshWeather() const {
  return has_fresh_weather_;
}

bool WeatherClient::fetchWeather(int8_t *temp_c_out, uint8_t *weather_code_out, uint16_t *sunset_min_out) {
  WiFiClientSecure secure_client;
  secure_client.setInsecure();

  HTTPClient http;
  const String url = buildWeatherUrl();
  const uint32_t start_ms = millis();
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
      log_->print("[WEATHER] request ms=");
      log_->println(millis() - start_ms);
    }
    http.end();
    return false;
  }

  const String response_body = http.getString();
  const size_t response_bytes = response_body.length();

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, response_body);
  http.end();
  if (error) {
    if (log_ != nullptr) {
      log_->print("[WEATHER] JSON parse failed: ");
      log_->println(error.c_str());
      log_->print("[WEATHER] response bytes=");
      log_->println(static_cast<unsigned int>(response_bytes));
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

  if (sunset_min_out != nullptr) {
    JsonVariant daily = doc["daily"];
    if (!daily.isNull()) {
      JsonVariant sunset_array = daily["sunset"];
      if (!sunset_array.isNull() && sunset_array.is<JsonArray>()) {
        const char* sunset_str = sunset_array[0].as<const char*>();
        if (sunset_str != nullptr) {
          // Format is usually YYYY-MM-DDTHH:MM
          const char* t_idx = strchr(sunset_str, 'T');
          if (t_idx != nullptr && strlen(t_idx) >= 6) {
            int hh = atoi(t_idx + 1);
            int mm = atoi(t_idx + 4);
            *sunset_min_out = hh * 60 + mm;
            if (log_ != nullptr) {
              log_->print("[WEATHER] Sunset parsed: ");
              log_->print(hh);
              log_->print(":");
              log_->println(mm);
            }
          }
        }
      }
    }
  }

  if (log_ != nullptr) {
    log_->print("[WEATHER] Download OK url=");
    log_->println(url);
    log_->print("[WEATHER] HTTP=");
    log_->print(http_code);
    log_->print(" bytes=");
    log_->print(static_cast<unsigned int>(response_bytes));
    log_->print(" ms=");
    log_->println(millis() - start_ms);
    log_->print("[WEATHER] Parsed ");
    log_->print(static_cast<int>(lroundf(temp_c)));
    log_->print("C code=");
    log_->println(weather_code);
  }

  has_fresh_weather_ = true;

  return true;
}

bool WeatherClient::fetchGreeting(char *greeting_out, size_t max_len) {
  HTTPClient http;
  const String url = "http://api.adviceslip.com/advice";
  const uint32_t start_ms = millis();
  if (!http.begin(url)) {
    if (log_ != nullptr) log_->println("[GREETING] HTTP begin failed");
    return false;
  }

  http.setTimeout(5000);
  const int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    if (log_ != nullptr) {
      log_->print("[GREETING] HTTP GET failed: ");
      log_->println(http_code);
    }
    http.end();
    return false;
  }

  const String response_body = http.getString();
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, response_body);
  http.end();
  
  if (error) {
    if (log_ != nullptr) log_->println("[GREETING] JSON parse failed");
    return false;
  }

  const char* advice = doc["slip"]["advice"];
  if (advice && greeting_out) {
    strlcpy(greeting_out, advice, max_len);
    if (log_ != nullptr) {
      log_->print("[GREETING] OK ms=");
      log_->println(millis() - start_ms);
      log_->print("[GREETING] Text: ");
      log_->println(greeting_out);
    }
    return true;
  }
  return false;
}

}  // namespace orb
