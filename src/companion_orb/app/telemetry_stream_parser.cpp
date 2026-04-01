#include "telemetry_stream_parser.h"

#include <string.h>

#include "telemetry_protocol.h"

namespace orb {

namespace {

bool isValidPacketType(uint8_t type) {
  return type == static_cast<uint8_t>(transport::PacketType::FastTelemetry) ||
         type == static_cast<uint8_t>(transport::PacketType::StatusTelemetry);
}

}  // namespace

TelemetryStreamParser::TelemetryStreamParser() { reset(); }

void TelemetryStreamParser::reset() {
  latest_ = telemetry::makeSimulatedTelemetry(0, 0, 900, 0, 84, 12600, 78);
  has_fresh_ = false;
  buffered_bytes_ = 0;
  dropped_bytes_ = 0;
}

void TelemetryStreamParser::pushBytes(const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    appendByte(data[index]);
  }
}

bool TelemetryStreamParser::hasFreshTelemetry() const { return has_fresh_; }

telemetry::DashboardTelemetry TelemetryStreamParser::takeTelemetry() {
  has_fresh_ = false;
  return latest_;
}

size_t TelemetryStreamParser::droppedByteCount() const { return dropped_bytes_; }

void TelemetryStreamParser::appendByte(uint8_t byte) {
  if (buffered_bytes_ == sizeof(buffer_)) {
    dropBytes(1);
    ++dropped_bytes_;
  }

  buffer_[buffered_bytes_] = byte;
  ++buffered_bytes_;
  consumeBuffer();
}

void TelemetryStreamParser::consumeBuffer() {
  while (buffered_bytes_ >= sizeof(transport::FrameHeader)) {
    size_t sync_offset = 0;
    while (sync_offset + 1U < buffered_bytes_ &&
           (buffer_[sync_offset] != transport::kFrameSync0 ||
            buffer_[sync_offset + 1U] != transport::kFrameSync1)) {
      ++sync_offset;
    }

    if (sync_offset > 0) {
      dropBytes(sync_offset);
      dropped_bytes_ += sync_offset;
      continue;
    }

    transport::FrameHeader header{};
    memcpy(&header, buffer_, sizeof(header));

    if (header.version != transport::kProtocolVersion || !isValidPacketType(header.type)) {
      dropBytes(1);
      ++dropped_bytes_;
      continue;
    }

    const size_t frame_size = sizeof(transport::FrameHeader) + header.payload_length + 1U;
    if (frame_size > sizeof(buffer_)) {
      dropBytes(1);
      ++dropped_bytes_;
      continue;
    }

    if (buffered_bytes_ < frame_size) {
      return;
    }

    const uint8_t expected_checksum = transport::computeChecksum(
        buffer_ + 2, (sizeof(transport::FrameHeader) - 2U) + header.payload_length);
    const uint8_t observed_checksum = buffer_[frame_size - 1U];
    if (expected_checksum != observed_checksum) {
      dropBytes(1);
      ++dropped_bytes_;
      continue;
    }

    const uint8_t *payload_bytes = buffer_ + sizeof(transport::FrameHeader);
    if (header.type == static_cast<uint8_t>(transport::PacketType::FastTelemetry) &&
        header.payload_length == sizeof(transport::FastTelemetryPayload)) {
      applyFastPayload(payload_bytes);
      has_fresh_ = true;
      dropBytes(frame_size);
      continue;
    }

    if (header.type == static_cast<uint8_t>(transport::PacketType::StatusTelemetry) &&
        header.payload_length == sizeof(transport::StatusTelemetryPayload)) {
      applyStatusPayload(payload_bytes);
      has_fresh_ = true;
      dropBytes(frame_size);
      continue;
    }

    dropBytes(1);
    ++dropped_bytes_;
  }
}

void TelemetryStreamParser::dropBytes(size_t count) {
  if (count >= buffered_bytes_) {
    buffered_bytes_ = 0;
    return;
  }

  memmove(buffer_, buffer_ + count, buffered_bytes_ - count);
  buffered_bytes_ -= count;
}

void TelemetryStreamParser::applyFastPayload(const uint8_t *payload_bytes) {
  transport::FastTelemetryPayload payload{};
  memcpy(&payload, payload_bytes, sizeof(payload));

  latest_.sequence = payload.sequence;
  latest_.uptime_ms = payload.uptime_ms;
  latest_.rpm = payload.rpm;
  latest_.speed_kph = payload.speed_kph;
  latest_.longitudinal_accel_mg = payload.longitudinal_accel_mg;
  latest_.drive_mode = static_cast<telemetry::DriveMode>(payload.drive_mode);
  latest_.companion_mood = static_cast<telemetry::CompanionMood>(payload.companion_mood);
  latest_.gear = static_cast<telemetry::TransmissionGear>(payload.gear);
  latest_.headlights_on = payload.headlights_on != 0;
}

void TelemetryStreamParser::applyStatusPayload(const uint8_t *payload_bytes) {
  transport::StatusTelemetryPayload payload{};
  memcpy(&payload, payload_bytes, sizeof(payload));

  latest_.sequence = payload.sequence;
  latest_.coolant_temp_c = payload.coolant_temp_c;
  latest_.battery_mv = payload.battery_mv;
  latest_.fuel_level_pct = payload.fuel_level_pct;
  latest_.estimated_range_km = payload.estimated_range_km;
}

}  // namespace orb