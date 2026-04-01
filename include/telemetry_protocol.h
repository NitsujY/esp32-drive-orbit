#pragma once

#include <Arduino.h>
#include <string.h>

#include "shared_data.h"

namespace transport {

constexpr uint8_t kFrameSync0 = 0xA5;
constexpr uint8_t kFrameSync1 = 0x5A;
constexpr uint8_t kProtocolVersion = 1;

enum class PacketType : uint8_t {
  FastTelemetry = 1,
  StatusTelemetry = 2,
};

struct FrameHeader {
  uint8_t sync0;
  uint8_t sync1;
  uint8_t version;
  uint8_t type;
  uint8_t payload_length;
} __attribute__((packed));

struct FastTelemetryPayload {
  uint32_t sequence;
  uint32_t uptime_ms;
  int16_t rpm;
  int16_t speed_kph;
  int16_t longitudinal_accel_mg;
  uint8_t drive_mode;
  uint8_t companion_mood;
  uint8_t gear;
  uint8_t headlights_on;
} __attribute__((packed));

struct StatusTelemetryPayload {
  uint32_t sequence;
  int16_t coolant_temp_c;
  uint16_t battery_mv;
  uint8_t fuel_level_pct;
  uint16_t estimated_range_km;
} __attribute__((packed));

static_assert(sizeof(FrameHeader) == 5, "Unexpected frame header size");
static_assert(sizeof(FastTelemetryPayload) == 18, "Unexpected fast telemetry size");
static_assert(sizeof(StatusTelemetryPayload) == 11, "Unexpected status telemetry size");

inline uint8_t computeChecksum(const uint8_t *data, size_t length) {
  uint8_t checksum = 0;
  for (size_t index = 0; index < length; ++index) {
    checksum ^= data[index];
  }

  return checksum;
}

template <typename Payload>
size_t encodeFrame(PacketType type,
                   const Payload &payload,
                   uint8_t *buffer,
                   size_t buffer_size) {
  const size_t frame_size = sizeof(FrameHeader) + sizeof(Payload) + sizeof(uint8_t);
  if (buffer_size < frame_size) {
    return 0;
  }

  const FrameHeader header = {
      kFrameSync0,
      kFrameSync1,
      kProtocolVersion,
      static_cast<uint8_t>(type),
      static_cast<uint8_t>(sizeof(Payload)),
  };

  memcpy(buffer, &header, sizeof(header));
  memcpy(buffer + sizeof(header), &payload, sizeof(payload));

  buffer[frame_size - 1] = computeChecksum(
      buffer + 2, (sizeof(header) - 2U) + sizeof(payload));

  return frame_size;
}

inline FastTelemetryPayload makeFastTelemetryPayload(const telemetry::DashboardTelemetry &telemetry) {
  FastTelemetryPayload payload{};
  payload.sequence = telemetry.sequence;
  payload.uptime_ms = telemetry.uptime_ms;
  payload.rpm = telemetry.rpm;
  payload.speed_kph = telemetry.speed_kph;
  payload.longitudinal_accel_mg = telemetry.longitudinal_accel_mg;
  payload.drive_mode = static_cast<uint8_t>(telemetry.drive_mode);
  payload.companion_mood = static_cast<uint8_t>(telemetry.companion_mood);
  payload.gear = static_cast<uint8_t>(telemetry.gear);
  payload.headlights_on = telemetry.headlights_on ? 1 : 0;
  return payload;
}

inline StatusTelemetryPayload makeStatusTelemetryPayload(const telemetry::DashboardTelemetry &telemetry) {
  StatusTelemetryPayload payload{};
  payload.sequence = telemetry.sequence;
  payload.coolant_temp_c = telemetry.coolant_temp_c;
  payload.battery_mv = telemetry.battery_mv;
  payload.fuel_level_pct = telemetry.fuel_level_pct;
  payload.estimated_range_km = telemetry.estimated_range_km;
  return payload;
}

inline size_t encodeFastTelemetryFrame(const telemetry::DashboardTelemetry &telemetry,
                                       uint8_t *buffer,
                                       size_t buffer_size) {
  return encodeFrame(PacketType::FastTelemetry, makeFastTelemetryPayload(telemetry), buffer,
                     buffer_size);
}

inline size_t encodeStatusTelemetryFrame(const telemetry::DashboardTelemetry &telemetry,
                                         uint8_t *buffer,
                                         size_t buffer_size) {
  return encodeFrame(PacketType::StatusTelemetry, makeStatusTelemetryPayload(telemetry), buffer,
                     buffer_size);
}

constexpr size_t kFastTelemetryFrameSize =
    sizeof(FrameHeader) + sizeof(FastTelemetryPayload) + sizeof(uint8_t);
constexpr size_t kStatusTelemetryFrameSize =
    sizeof(FrameHeader) + sizeof(StatusTelemetryPayload) + sizeof(uint8_t);
constexpr size_t kMaxTelemetryFrameSize =
    kFastTelemetryFrameSize > kStatusTelemetryFrameSize ? kFastTelemetryFrameSize
                                                        : kStatusTelemetryFrameSize;

}  // namespace transport