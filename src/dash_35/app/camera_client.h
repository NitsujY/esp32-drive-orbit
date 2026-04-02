// camera_client.h
#pragma once

#include <Arduino.h>
#include <vector>

#include "shared_data.h"

namespace app {

struct CameraPoint {
  float lat = 0.0f;
  float lon = 0.0f;
};

class CameraClient {
 public:
  void begin(Print &log);
  void poll(uint32_t now_ms, bool wifi_connected, telemetry::DashboardTelemetry &telemetry);

 private:
  bool fetchCameras();
  void computeNearest(telemetry::DashboardTelemetry &telemetry);
  void loadCache();
  void saveCache();

  Print *log_ = nullptr;
  std::vector<CameraPoint> cameras_;
  uint32_t last_fetch_ms_ = 0;
  bool last_wifi_connected_ = false;
  uint32_t connected_since_ms_ = 0;
  bool first_fetch_pending_ = false;
  uint16_t last_nearest_m_ = telemetry::kNearestCameraUnknown;
};

}  // namespace app
