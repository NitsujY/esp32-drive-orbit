#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace app {

class TelemetryTransmitter {
 public:
  void begin(Print &log_output);
  void publish(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms);

 private:
  void publishFastFrame(const telemetry::DashboardTelemetry &telemetry);
  void publishStatusFrame(const telemetry::DashboardTelemetry &telemetry);
  void logFrame(const char *label, const uint8_t *frame, size_t frame_size) const;

  Print *log_output_ = nullptr;
  uint32_t last_fast_publish_ms_ = 0;
  uint32_t last_status_publish_ms_ = 0;
  bool emitted_initial_status_ = false;
};

}  // namespace app