#pragma once

#include <Arduino.h>

#include "car_telemetry.h"

namespace app {

class CarTelemetryStore {
 public:
  void publish(const telemetry::CarTelemetry &snapshot);
  telemetry::CarTelemetry snapshot() const;

 private:
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  telemetry::CarTelemetry snapshot_ = telemetry::makeInitialCarTelemetry();
};

}  // namespace app