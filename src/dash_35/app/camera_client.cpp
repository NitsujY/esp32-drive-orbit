#include "camera_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>

#include "wifi_config.h"

namespace app {

namespace {
constexpr uint32_t kFirstFetchDelayMs = 5000;
constexpr uint32_t kFetchIntervalMs = 300000;  // 5 minutes
const char *kCachePath = "/cameras.json";

static double deg2rad(double d) { return d * M_PI / 180.0; }

static uint32_t haversine_m(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;
  const double dlat = deg2rad(lat2 - lat1);
  const double dlon = deg2rad(lon2 - lon1);
  const double a = sin(dlat / 2.0) * sin(dlat / 2.0) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) *
                   sin(dlon / 2.0) * sin(dlon / 2.0);
  const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return static_cast<uint32_t>(R * c + 0.5);
}

}  // namespace

void CameraClient::begin(Print &log) {
  log_ = &log;
  if (!LittleFS.begin()) {
    if (log_) log_->println("[CAM] LittleFS mount failed");
  } else {
    loadCache();
  }
}

void CameraClient::poll(uint32_t now_ms, bool wifi_connected, telemetry::DashboardTelemetry &telemetry) {
  // Keep telemetry field updated with cached/known nearest camera distance.
  if (!wifi_connected) {
    last_wifi_connected_ = false;
    connected_since_ms_ = 0;
    first_fetch_pending_ = false;
    computeNearest(telemetry);
    return;
  }

  if (!last_wifi_connected_) {
    connected_since_ms_ = now_ms;
    first_fetch_pending_ = true;
    if (log_) log_->println("[CAM] Wi-Fi up, scheduling fetch");
  }
  last_wifi_connected_ = true;

  const bool first_fetch_due = first_fetch_pending_ && connected_since_ms_ > 0 &&
                               (now_ms - connected_since_ms_ >= kFirstFetchDelayMs);
  const bool periodic_fetch_due = last_fetch_ms_ > 0 &&
                                  (now_ms - last_fetch_ms_ >= kFetchIntervalMs);

  if (first_fetch_due || periodic_fetch_due || last_fetch_ms_ == 0) {
    if (fetchCameras()) {
      last_fetch_ms_ = now_ms;
      first_fetch_pending_ = false;
      saveCache();
    } else if (last_fetch_ms_ == 0) {
      last_fetch_ms_ = now_ms;
    }
  }

  computeNearest(telemetry);
}

void CameraClient::computeNearest(telemetry::DashboardTelemetry &telemetry) {
  if (cameras_.empty()) {
    last_nearest_m_ = telemetry::kNearestCameraUnknown;
    telemetry.nearest_camera_m = last_nearest_m_;
    return;
  }

  uint32_t best = UINT32_MAX;
  for (const auto &c : cameras_) {
    const uint32_t d = haversine_m(LOCATION_LAT, LOCATION_LON, c.lat, c.lon);
    if (d < best) best = d;
  }

  last_nearest_m_ = (best == UINT32_MAX) ? telemetry::kNearestCameraUnknown : static_cast<uint16_t>(best > 0xFFFE ? 0xFFFE : best);
  telemetry.nearest_camera_m = last_nearest_m_;
}

void CameraClient::loadCache() {
  if (!LittleFS.exists(kCachePath)) return;
  File f = LittleFS.open(kCachePath, "r");
  if (!f) return;
  const size_t size = f.size();
  if (size == 0) {
    f.close();
    return;
  }
  std::unique_ptr<char[]> buf(new char[size + 1]);
  f.readBytes(buf.get(), size);
  buf[size] = '\0';
  f.close();

  DynamicJsonDocument doc(16 * 1024);
  const DeserializationError err = deserializeJson(doc, buf.get());
  if (err) {
    if (log_) {
      log_->print("[CAM] Cache parse failed: ");
      log_->println(err.c_str());
    }
    return;
  }

  cameras_.clear();
  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    CameraPoint p{};
    p.lat = obj["lat"] | 0.0f;
    p.lon = obj["lon"] | 0.0f;
    if (p.lat != 0.0f || p.lon != 0.0f) cameras_.push_back(p);
  }

  if (log_) {
    log_->print("[CAM] Loaded camera cache, entries=");
    log_->println(cameras_.size());
  }
}

void CameraClient::saveCache() {
  DynamicJsonDocument doc(16 * 1024);
  JsonArray arr = doc.to<JsonArray>();
  for (const auto &c : cameras_) {
    JsonObject obj = arr.createNestedObject();
    obj["lat"] = c.lat;
    obj["lon"] = c.lon;
  }

  File f = LittleFS.open(kCachePath, "w");
  if (!f) {
    if (log_) log_->println("[CAM] Cache save failed");
    return;
  }
  serializeJson(doc, f);
  f.close();
  if (log_) {
    log_->print("[CAM] Saved cache entries=");
    log_->println(cameras_.size());
  }
}

bool CameraClient::fetchCameras() {
  if (log_) log_->println("[CAM] Fetching cameras from Overpass");

  HTTPClient http;
  const String query = String("[out:json];node[\"highway\"=\"speed_camera\"](around:5000,") +
                       String(LOCATION_LAT, 4) + "," + String(LOCATION_LON, 4) + ");out;";

  // POST the query to the interpreter endpoint.
  if (!http.begin("http://overpass-api.de/api/interpreter")) {
    if (log_) log_->println("[CAM] HTTP begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const String body = String("data=") + query;
  http.setTimeout(7000);
  const int code = http.POST(body);
  if (code != HTTP_CODE_OK) {
    if (log_) {
      log_->print("[CAM] HTTP POST failed: ");
      log_->println(code);
    }
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(32 * 1024);
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    if (log_) {
      log_->print("[CAM] JSON parse failed: ");
      log_->println(err.c_str());
    }
    return false;
  }

  if (!doc.containsKey("elements")) {
    if (log_) log_->println("[CAM] No elements in Overpass response");
    return false;
  }

  cameras_.clear();
  JsonArray elements = doc["elements"].as<JsonArray>();
  for (JsonObject obj : elements) {
    double lat = obj["lat"] | 0.0;
    double lon = obj["lon"] | 0.0;
    if (lat == 0.0 && lon == 0.0) continue;
    CameraPoint p;
    p.lat = static_cast<float>(lat);
    p.lon = static_cast<float>(lon);
    cameras_.push_back(p);
    if (cameras_.size() > 1000) break;
  }

  if (log_) {
    log_->print("[CAM] Fetched cameras=");
    log_->println(cameras_.size());
  }

  return true;
}

}  // namespace app
