#include "car_telemetry_store.h"

namespace app {

void CarTelemetryStore::publish(const telemetry::CarTelemetry &snapshot) {
  portENTER_CRITICAL(&mux_);
  snapshot_ = snapshot;
  portEXIT_CRITICAL(&mux_);
}

telemetry::CarTelemetry CarTelemetryStore::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const telemetry::CarTelemetry current = snapshot_;
  portEXIT_CRITICAL(&mux_);
  return current;
}

}  // namespace app