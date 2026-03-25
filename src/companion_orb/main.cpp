#include <Arduino.h>

#include "shared_data.h"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;

uint32_t last_heartbeat_ms = 0;

void logPreviewContract() {
  const telemetry::DashboardTelemetry preview = telemetry::makeSimulatedTelemetry(
      0, millis(), 1600, 24, 86, 12480, 64);

  Serial.print("[ORB] Placeholder target ready. mode=");
  Serial.print(static_cast<uint8_t>(preview.drive_mode));
  Serial.print(" mood=");
  Serial.println(static_cast<uint8_t>(preview.companion_mood));
}

}  // namespace

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("[BOOT] companion_orb placeholder starting");
  Serial.println("[BOOT] Transport and round display stack deferred by plan");

  logPreviewContract();
}

void loop() {
  const uint32_t now = millis();

  if (now - last_heartbeat_ms >= kHeartbeatIntervalMs) {
    last_heartbeat_ms = now;
    Serial.println("[ORB] Waiting for later-stage transport decision");
  }
}