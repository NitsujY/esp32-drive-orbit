#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace orb {

class TelemetryStreamParser {
 public:
  TelemetryStreamParser();

  void reset();
  void pushBytes(const uint8_t *data, size_t length);

  bool hasFreshTelemetry() const;
  telemetry::DashboardTelemetry takeTelemetry();
  size_t droppedByteCount() const;

 private:
  void appendByte(uint8_t byte);
  void consumeBuffer();
  void dropBytes(size_t count);
  void applyFastPayload(const uint8_t *payload_bytes);
  void applyStatusPayload(const uint8_t *payload_bytes);

  telemetry::DashboardTelemetry latest_;
  bool has_fresh_;
  uint8_t buffer_[96];
  size_t buffered_bytes_;
  size_t dropped_bytes_;
};

}  // namespace orb