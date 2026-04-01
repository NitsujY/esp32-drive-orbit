#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "app/dashboard_display.h"
#include "app/elm327_client.h"
#include "app/app_state.h"
#include "app/telemetry_transmitter.h"
#include "app/vehicle_profiles/vehicle_profile.h"
#include "app/weather_client.h"
#include "app/wifi_manager.h"
#include "cyber_theme.h"
#include "espnow_transport.h"
#include "shared_data.h"

namespace {

constexpr uint32_t kTelemetryIntervalMs = 100;

uint32_t last_publish_ms = 0;
app::AppState app_state = app::makeInitialState();
app::TelemetryTransmitter telemetry_transmitter;
app::DashboardDisplay dashboard_display;
app::Elm327Client elm327_client;
app::WifiManager wifi_manager;
app::WeatherClient weather_client;

app::ObdConnectionState mapObdConnectionState(app::Elm327Client::ConnectionState state) {
  switch (state) {
    case app::Elm327Client::ConnectionState::Disconnected:
      return app::ObdConnectionState::Disconnected;
    case app::Elm327Client::ConnectionState::Connecting:
      return app::ObdConnectionState::Connecting;
    case app::Elm327Client::ConnectionState::Live:
      return app::ObdConnectionState::Live;
  }
  return app::ObdConnectionState::Disconnected;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] dash_35 starting");
  Serial.print("[BOOT] Vehicle profile: ");
  Serial.println(app::vehicle_profiles::activeProfile().display_name);

  const size_t psram_size = ESP.getPsramSize();
  if (psram_size > 0) {
    Serial.print("[BOOT] PSRAM: ");
    Serial.print(psram_size);
    Serial.println(" bytes");
  }

  telemetry_transmitter.begin(Serial);
  dashboard_display.begin(Serial);
  elm327_client.begin(Serial);

  if (espnow::beginTransport() && espnow::addBroadcastPeer()) {
    Serial.println("[BOOT] ESP-NOW transport ready");
  } else {
    Serial.println("[BOOT] ESP-NOW init failed — companion link unavailable");
  }

  wifi_manager.begin(Serial);
  weather_client.begin(Serial);

  app_state = app::makeInitialState();
  app_state.psram_detected = psram_size > 0;
  app_state.psram_size_bytes = psram_size;
}

void loop() {
  const uint32_t now = millis();
  elm327_client.poll(now, app_state.telemetry);
  wifi_manager.poll(now);

  if (now - last_publish_ms >= kTelemetryIntervalMs) {
    last_publish_ms = now;
    if (elm327_client.hasFreshTelemetry(now)) {
      const app::ObdConnectionState next_obd_state = mapObdConnectionState(elm327_client.connectionState());
      if (app_state.view_state.obd_connection_state != app::ObdConnectionState::Live &&
          next_obd_state == app::ObdConnectionState::Live) {
        app::resetTripMetrics(app_state.view_state, "Live OBD connected. Trip tracking reset.");
        app_state.last_trip_accum_ms = 0;
      }
      const int16_t previous_speed_kph = app_state.previous_speed_kph;
      app_state.telemetry.sequence += 1;
      app_state.telemetry.uptime_ms = now;
      app::accumulateTripDistance(app_state, now, true);
      app::vehicle_profiles::refreshTelemetry(app_state.telemetry);
      if (app_state.last_speed_sample_ms != 0 && now > app_state.last_speed_sample_ms) {
        const float dt_s = static_cast<float>(now - app_state.last_speed_sample_ms) / 1000.0f;
        if (dt_s > 0.05f && dt_s < 2.0f) {
          const float delta_kph = static_cast<float>(app_state.telemetry.speed_kph - previous_speed_kph);
          const float delta_mps = delta_kph / 3.6f;
          const float accel_g = (delta_mps / dt_s) / 9.80665f;
          app_state.telemetry.longitudinal_accel_mg =
              static_cast<int16_t>(lroundf(accel_g * 1000.0f));
        } else {
          app_state.telemetry.longitudinal_accel_mg = 0;
        }
      } else {
        app_state.telemetry.longitudinal_accel_mg = 0;
      }
      app_state.last_speed_sample_ms = now;
      app::updateDashboardViewState(app_state.view_state, app_state.telemetry, previous_speed_kph);
      app_state.view_state.obd_connection_state = next_obd_state;
      app_state.previous_speed_kph = app_state.telemetry.speed_kph;
    } else {
      app_state.view_state.obd_connection_state = mapObdConnectionState(elm327_client.connectionState());
      app::maintainIdleState(app_state, now);
      app_state.telemetry.longitudinal_accel_mg = 0;
    }
    weather_client.poll(now, wifi_manager.isConnected(), app_state.telemetry);
    telemetry_transmitter.publish(app_state.telemetry, now);
  }

  // Sync light/dark mode from headlight state.
  theme::darkMode() = app_state.telemetry.headlights_on;

  dashboard_display.render(app_state.telemetry, app_state.view_state, app_state.psram_detected,
                           app_state.psram_size_bytes, now);
}
