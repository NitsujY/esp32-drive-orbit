#include <Arduino.h>

#include "shared_data.h"
#include "telemetry_protocol.h"
#include "app/telemetry_stream_parser.h"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;
constexpr uint32_t kFastPacketIntervalMs = 500;
constexpr uint32_t kStatusPacketIntervalMs = 10000;

uint32_t last_fast_packet_ms = 0;
uint32_t last_status_packet_ms = 0;

uint32_t last_heartbeat_ms = 0;
bool speed_increasing = true;

orb::TelemetryStreamParser parser;
telemetry::DashboardTelemetry source_telemetry =
    telemetry::makeSimulatedTelemetry(0, 0, 900, 0, 84, 12600, 78);

void advanceMotionState(uint32_t now_ms) {
  int16_t next_speed = source_telemetry.speed_kph;
  if (speed_increasing) {
    next_speed += 3;
    if (next_speed >= 92) {
      speed_increasing = false;
    }
  } else {
    next_speed -= 2;
    if (next_speed <= 0) {
      next_speed = 0;
      speed_increasing = true;
    }
  }

  const int16_t next_rpm = static_cast<int16_t>(780 + (next_speed * 18) + (speed_increasing ? 70 : 0));
  source_telemetry = telemetry::makeSimulatedTelemetry(
      source_telemetry.sequence + 1U, now_ms, next_rpm, next_speed, source_telemetry.coolant_temp_c,
      source_telemetry.battery_mv, source_telemetry.fuel_level_pct);
}

void advanceStatusState() {
  if (speed_increasing && source_telemetry.coolant_temp_c < 92) {
    ++source_telemetry.coolant_temp_c;
  }

  if (!speed_increasing && source_telemetry.coolant_temp_c > 82) {
    --source_telemetry.coolant_temp_c;
  }

  if (speed_increasing && source_telemetry.battery_mv > 12380) {
    source_telemetry.battery_mv -= 8;
  } else if (!speed_increasing && source_telemetry.battery_mv < 12600) {
    source_telemetry.battery_mv += 4;
  }

  if (source_telemetry.fuel_level_pct > 12) {
    --source_telemetry.fuel_level_pct;
  }

  source_telemetry.estimated_range_km =
      telemetry::estimateRangeKm(source_telemetry.fuel_level_pct, source_telemetry.speed_kph);
}

void feedFrameInChunks(const uint8_t *frame, size_t length, bool inject_noise) {
  if (inject_noise) {
    const uint8_t noise[] = {0x00, 0x7E, 0x19};
    parser.pushBytes(noise, sizeof(noise));
  }

  size_t offset = 0;
  size_t chunk_seed = 0;
  while (offset < length) {
    const size_t remaining = length - offset;
    const size_t requested_chunk = (chunk_seed % 4U) + 1U;
    const size_t actual_chunk = remaining < requested_chunk ? remaining : requested_chunk;
    parser.pushBytes(frame + offset, actual_chunk);
    offset += actual_chunk;
    ++chunk_seed;
  }
}

void publishFastPacket(uint32_t now_ms) {
  advanceMotionState(now_ms);

  uint8_t frame[transport::kFastTelemetryFrameSize] = {0};
  const size_t frame_size = transport::encodeFastTelemetryFrame(source_telemetry, frame, sizeof(frame));
  feedFrameInChunks(frame, frame_size, false);
}

void publishStatusPacket() {
  advanceStatusState();

  uint8_t frame[transport::kStatusTelemetryFrameSize] = {0};
  const size_t frame_size =
      transport::encodeStatusTelemetryFrame(source_telemetry, frame, sizeof(frame));
  const bool inject_noise = (source_telemetry.sequence % 3U) == 0U;
  feedFrameInChunks(frame, frame_size, inject_noise);
}

void logParsedTelemetry(const telemetry::DashboardTelemetry &snapshot) {
  Serial.print("[ORB] parsed seq=");
  Serial.print(snapshot.sequence);
  Serial.print(" speed=");
  Serial.print(snapshot.speed_kph);
  Serial.print(" rpm=");
  Serial.print(snapshot.rpm);
  Serial.print(" gear=");
  Serial.print(static_cast<uint8_t>(snapshot.gear));
  Serial.print(" fuel=");
  Serial.print(snapshot.fuel_level_pct);
  Serial.print("% range=");
  Serial.print(snapshot.estimated_range_km);
  Serial.print("km dropped=");
  Serial.println(parser.droppedByteCount());
}

void logPreviewContract() {
  const telemetry::DashboardTelemetry preview = telemetry::makeSimulatedTelemetry(
      0, millis(), 1600, 24, 86, 12480, 64);

  Serial.print("[ORB] Placeholder target ready. mode=");
  Serial.print(static_cast<uint8_t>(preview.drive_mode));
  Serial.print(" gear=");
  Serial.print(static_cast<uint8_t>(preview.gear));
  Serial.print(" range=");
  Serial.print(preview.estimated_range_km);
  Serial.print(" mood=");
  Serial.println(static_cast<uint8_t>(preview.companion_mood));
}

}  // namespace

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("[BOOT] companion_orb placeholder starting");
  Serial.println("[BOOT] companion_orb parser validation mode");
  Serial.println("[BOOT] Validating framed telemetry parsing before real transport wiring");

  logPreviewContract();
}

void loop() {
  const uint32_t now = millis();

  if (now - last_fast_packet_ms >= kFastPacketIntervalMs) {
    last_fast_packet_ms = now;
    publishFastPacket(now);
  }

  if (now - last_status_packet_ms >= kStatusPacketIntervalMs) {
    last_status_packet_ms = now;
    publishStatusPacket();
  }

  if (parser.hasFreshTelemetry()) {
    logParsedTelemetry(parser.takeTelemetry());
  }

  if (now - last_heartbeat_ms >= kHeartbeatIntervalMs) {
    last_heartbeat_ms = now;
    Serial.println("[ORB] Parser active; awaiting real transport hookup in a later slice");
  }
}