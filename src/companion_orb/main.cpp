#include <Arduino.h>
#include <esp_now.h>

#include "espnow_transport.h"
#include "cyber_theme.h"
#include "shared_data.h"
#include "app/telemetry_stream_parser.h"
#include "app/orb_display.h"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kStaleThresholdMs = 5000;

uint32_t last_heartbeat_ms = 0;
uint32_t last_receive_ms = 0;
uint32_t packets_received = 0;

orb::TelemetryStreamParser parser;
orb::OrbDisplay display;

// ESP-NOW receive callback — runs in Wi-Fi task context, keep it fast.
void onDataReceived(const uint8_t * /*mac_addr*/, const uint8_t *data, int data_len) {
  if (data_len > 0) {
    parser.pushBytes(data, static_cast<size_t>(data_len));
    last_receive_ms = millis();
    ++packets_received;
  }
}

void logParsedTelemetry(const telemetry::DashboardTelemetry &snapshot) {
  Serial.print("[ORB] seq=");
  Serial.print(snapshot.sequence);
  Serial.print(" spd=");
  Serial.print(snapshot.speed_kph);
  Serial.print(" rpm=");
  Serial.print(snapshot.rpm);
  Serial.print(" gear=");
  Serial.print(static_cast<uint8_t>(snapshot.gear));
  Serial.print(" fuel=");
  Serial.print(snapshot.fuel_level_pct);
  Serial.print("% range=");
  Serial.print(snapshot.estimated_range_km);
  Serial.print("km drop=");
  Serial.println(parser.droppedByteCount());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] companion_orb starting");

  display.begin(Serial);

  if (espnow::beginTransport()) {
    esp_now_register_recv_cb(onDataReceived);
    Serial.println("[BOOT] ESP-NOW receiver ready — waiting for dash_35 broadcast");
  } else {
    Serial.println("[BOOT] ESP-NOW init failed");
  }

  Serial.print("[BOOT] MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  const uint32_t now = millis();

  // A live signal means we've received a packet recently (within stale threshold).
  const bool has_signal = (last_receive_ms != 0) && (now - last_receive_ms <= kStaleThresholdMs);

  if (parser.hasFreshTelemetry()) {
    const telemetry::DashboardTelemetry snapshot = parser.takeTelemetry();
    theme::darkMode() = snapshot.headlights_on;
    logParsedTelemetry(snapshot);
    display.render(snapshot, now);
  } else if (!has_signal) {
    // No connection to the gateway (never connected, or stream went stale):
    // keep the display alive with a waiting animation.
    display.renderNoSignal(now);
  }

  if (now - last_heartbeat_ms >= kHeartbeatIntervalMs) {
    last_heartbeat_ms = now;

    if (last_receive_ms == 0) {
      Serial.println("[ORB] Listening for dash_35 ESP-NOW packets...");
    } else if (now - last_receive_ms > kStaleThresholdMs) {
      Serial.print("[ORB] No data for ");
      Serial.print((now - last_receive_ms) / 1000U);
      Serial.print("s (total pkts=");
      Serial.print(packets_received);
      Serial.println(")");
    } else {
      Serial.print("[ORB] Link active, pkts=");
      Serial.println(packets_received);
    }
  }
}
