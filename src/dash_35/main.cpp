#include <Arduino.h>
#include <esp_heap_caps.h>

#include "app/app_state.h"
#include "shared_data.h"

namespace {

constexpr uint32_t kTelemetryIntervalMs = 100;

uint32_t last_publish_ms = 0;
app::AppState app_state = app::makeInitialState();

void printPsramInfo() {
  const size_t psram_size = ESP.getPsramSize();
  app_state.psram_detected = psram_size > 0;
  app_state.psram_size_bytes = psram_size;

  if (psram_size == 0) {
    Serial.println("[BOOT] PSRAM not detected.");
    return;
  }

  Serial.print("[BOOT] PSRAM detected: ");
  Serial.print(psram_size);
  Serial.println(" bytes");

  Serial.print("[BOOT] Free PSRAM: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");
}

void publishTelemetry() {
  const telemetry::DashboardTelemetry &packet = app_state.telemetry;

  Serial.print("[SIM] seq=");
  Serial.print(packet.sequence);
  Serial.print(" rpm=");
  Serial.print(packet.rpm);
  Serial.print(" speed=");
  Serial.print(packet.speed_kph);
  Serial.print(" coolant=");
  Serial.print(packet.coolant_temp_c);
  Serial.print("C battery=");
  Serial.print(packet.battery_mv);
  Serial.print("mV fuel=");
  Serial.print(packet.fuel_level_pct);
  Serial.print("% mode=");
  Serial.print(static_cast<uint8_t>(packet.drive_mode));
  Serial.print(" mood=");
  Serial.println(static_cast<uint8_t>(packet.companion_mood));
}

}  // namespace

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("[BOOT] dash_35 starting");
  Serial.println("[BOOT] Stage 1 local dashboard bring-up mode");
  Serial.println("[BOOT] Inter-board transport deferred");

  printPsramInfo();
  app_state = app::makeInitialState();
  app_state.psram_detected = ESP.getPsramSize() > 0;
  app_state.psram_size_bytes = ESP.getPsramSize();
}

void loop() {
  const uint32_t now = millis();

  if (now - last_publish_ms >= kTelemetryIntervalMs) {
    last_publish_ms = now;
    app::advanceSimulation(app_state, now);
    publishTelemetry();
  }
}