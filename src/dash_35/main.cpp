#include <Arduino.h>

#include "app/car_telemetry_store.h"
#include "app/dashboard_web_server.h"
#include "app/elm327_client.h"
#include "app/gateway_status_display.h"
#include "app/simulation_controller.h"
#include "app/weather_client.h"
#include "app/wifi_manager.h"
#include "car_telemetry.h"

namespace {

telemetry::CarTelemetry live_telemetry = telemetry::makeInitialCarTelemetry();
app::CarTelemetryStore telemetry_store;
app::DashboardWebServer dashboard_web_server(telemetry_store);
app::Elm327Client elm327_client;
app::GatewayStatusDisplay gateway_status_display;
app::SimulationController simulation_controller;
app::WeatherClient weather_client;
app::WifiManager wifi_manager;
int16_t previous_speed_kph = 0;
uint32_t last_speed_sample_ms = 0;
uint32_t last_trip_accum_ms = 0;
float trip_distance_km = 0.0f;
bool simulation_mode_active = false;

void updateDerivedTelemetry(uint32_t now_ms, bool fresh_sample) {
  live_telemetry.uptime_ms = now_ms;
  live_telemetry.wifi_connected = wifi_manager.isConnected();
  live_telemetry.obd_connected =
      elm327_client.connectionState() == app::Elm327Client::ConnectionState::Live;
  live_telemetry.telemetry_fresh = elm327_client.hasFreshTelemetry(now_ms);

  if (fresh_sample && last_speed_sample_ms != 0 && now_ms > last_speed_sample_ms) {
    const float dt_s = static_cast<float>(now_ms - last_speed_sample_ms) / 1000.0f;
    if (dt_s > 0.05f && dt_s < 2.0f) {
      const float delta_kph = static_cast<float>(live_telemetry.speed_kph - previous_speed_kph);
      const float delta_mps = delta_kph / 3.6f;
      const float accel_g = (delta_mps / dt_s) / 9.80665f;
      live_telemetry.longitudinal_accel_mg = static_cast<int16_t>(lroundf(accel_g * 1000.0f));
    } else {
      live_telemetry.longitudinal_accel_mg = 0;
    }
  } else if (!fresh_sample) {
    live_telemetry.longitudinal_accel_mg = 0;
  }

  telemetry::refreshDerivedTelemetry(live_telemetry);
}

void accumulateTripDistance(uint32_t now_ms, bool speed_fresh) {
  if (last_trip_accum_ms == 0) {
    last_trip_accum_ms = now_ms;
    return;
  }

  const uint32_t dt_ms = now_ms - last_trip_accum_ms;
  last_trip_accum_ms = now_ms;

  if (!speed_fresh || dt_ms <= 50U || dt_ms >= 2000U) {
    return;
  }

  const float dt_h = static_cast<float>(dt_ms) / 3600000.0f;
  trip_distance_km += static_cast<float>(live_telemetry.speed_kph) * dt_h;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] dash_35 headless gateway starting");

  elm327_client.begin(Serial);
  wifi_manager.begin(Serial);
  weather_client.begin(Serial);
  dashboard_web_server.begin(Serial);
  gateway_status_display.begin(Serial);
  simulation_controller.begin(Serial);
  telemetry_store.publish(live_telemetry);
}

void loop() {
  const uint32_t now = millis();
  simulation_controller.poll(now);
  wifi_manager.poll(now);
  const bool simulation_enabled = simulation_controller.enabled();
  if (simulation_enabled != simulation_mode_active) {
    simulation_mode_active = simulation_enabled;
    previous_speed_kph = live_telemetry.speed_kph;
    last_speed_sample_ms = 0;
    last_trip_accum_ms = 0;
    if (!simulation_enabled) {
      live_telemetry = telemetry::makeInitialCarTelemetry();
      live_telemetry.wifi_connected = wifi_manager.isConnected();
    }
  }

  bool fresh_sample = false;
  if (simulation_enabled) {
    fresh_sample = simulation_controller.apply(live_telemetry, now, wifi_manager.isConnected());
    accumulateTripDistance(now, fresh_sample);
  } else {
    elm327_client.poll(now, live_telemetry);
    weather_client.poll(now, wifi_manager.isConnected(), live_telemetry);

    fresh_sample = elm327_client.hasFreshTelemetry(now);
    if (fresh_sample) {
      live_telemetry.sequence += 1;
    }

    accumulateTripDistance(now, fresh_sample);
    updateDerivedTelemetry(now, fresh_sample);
  }

  telemetry_store.publish(live_telemetry);
  dashboard_web_server.poll(now, wifi_manager.isConnected());

  char access_label[48] = {0};
  if (wifi_manager.isConnected()) {
    const String ip = wifi_manager.localIp();
    snprintf(access_label, sizeof(access_label), "%s", ip.c_str());
  } else {
    snprintf(access_label, sizeof(access_label), "%s", wifi_manager.hostname());
  }
  gateway_status_display.render(live_telemetry, trip_distance_km, access_label,
                                dashboard_web_server.hasHostedUi(),
                                dashboard_web_server.usingEmbeddedFallback(),
                                simulation_enabled, now);

  previous_speed_kph = live_telemetry.speed_kph;
  last_speed_sample_ms = now;
}
