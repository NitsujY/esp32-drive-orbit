#include "telemetry_transmitter.h"

#include <esp_now.h>

#include "espnow_transport.h"
#include "telemetry_protocol.h"

namespace app {

namespace {

constexpr uint32_t kFastTelemetryIntervalMs = 500;
constexpr uint32_t kStatusTelemetryIntervalMs = 10000;
constexpr bool kLogFastFrames = false;
constexpr bool kLogStatusFrames = true;

}  // namespace

void TelemetryTransmitter::begin(Print &log_output) {
  log_output_ = &log_output;
  last_fast_publish_ms_ = 0;
  last_status_publish_ms_ = 0;
  emitted_initial_status_ = false;
  active_ = true;
}

bool TelemetryTransmitter::isActive() const { return active_; }

void TelemetryTransmitter::publish(const telemetry::CarTelemetry &telemetry, uint32_t now_ms) {
  if (log_output_ == nullptr) {
    return;
  }

  if (now_ms - last_fast_publish_ms_ >= kFastTelemetryIntervalMs) {
    last_fast_publish_ms_ = now_ms;
    publishFastFrame(telemetry);
  }

  if (!emitted_initial_status_ || now_ms - last_status_publish_ms_ >= kStatusTelemetryIntervalMs) {
    last_status_publish_ms_ = now_ms;
    emitted_initial_status_ = true;
    publishStatusFrame(telemetry);
  }
}

void TelemetryTransmitter::publishFastFrame(const telemetry::CarTelemetry &telemetry) {
  uint8_t frame[transport::kFastTelemetryFrameSize] = {0};
  const size_t frame_size = transport::encodeFastTelemetryFrame(telemetry, frame, sizeof(frame));
  if (frame_size > 0) {
    esp_now_send(espnow::kBroadcastAddress, frame, frame_size);
  }
  if (kLogFastFrames) {
    logFrame("fast", frame, frame_size);
  }
}

void TelemetryTransmitter::publishStatusFrame(const telemetry::CarTelemetry &telemetry) {
  uint8_t frame[transport::kStatusTelemetryFrameSize] = {0};
  const size_t frame_size = transport::encodeStatusTelemetryFrame(telemetry, frame, sizeof(frame));
  if (frame_size > 0) {
    esp_now_send(espnow::kBroadcastAddress, frame, frame_size);
  }
  if (kLogStatusFrames) {
    logFrame("status", frame, frame_size);
  }
}

void TelemetryTransmitter::logFrame(const char *label, const uint8_t *frame, size_t frame_size) const {
  if (log_output_ == nullptr || frame_size == 0) {
    return;
  }

  log_output_->print("[TX] ");
  log_output_->print(label);
  log_output_->print(" frame len=");
  log_output_->print(frame_size);
  log_output_->print(" bytes=");

  for (size_t index = 0; index < frame_size; ++index) {
    if (frame[index] < 16U) {
      log_output_->print('0');
    }

    log_output_->print(frame[index], HEX);
    if (index + 1U < frame_size) {
      log_output_->print(' ');
    }
  }

  log_output_->println();
}

}  // namespace app