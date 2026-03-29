#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include "wifi_config.h"

namespace app {

namespace {

constexpr uint32_t kRetryIntervalMs = 60000;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kLogIntervalMs = 30000;

}  // namespace

void WifiManager::begin(Print &log) {
  log_ = &log;

  // ESP-NOW requires STA mode, which is already set by espnow::beginTransport().
  // We just need to start the connection — WiFi.begin() in STA mode is additive.
  log_->print("[WIFI] Connecting to ");
  log_->println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Configure NTP timezone.
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  attempted_ = true;
  last_attempt_ms_ = millis();
}

void WifiManager::poll(uint32_t now_ms) {
  const bool was_connected = connected_;
  connected_ = (WiFi.status() == WL_CONNECTED);

  if (connected_ && !was_connected) {
    log_->print("[WIFI] Connected. IP: ");
    log_->println(WiFi.localIP());
  }

  if (!connected_ && was_connected) {
    log_->println("[WIFI] Disconnected from hotspot");
  }

  // Retry connection periodically.
  if (!connected_ && (now_ms - last_attempt_ms_ >= kRetryIntervalMs)) {
    log_->println("[WIFI] Retrying connection...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    last_attempt_ms_ = now_ms;
  }

  // Periodic status log.
  if (now_ms - last_log_ms_ >= kLogIntervalMs) {
    last_log_ms_ = now_ms;
    if (connected_) {
      log_->print("[WIFI] Connected, IP=");
      log_->print(WiFi.localIP());
      log_->print(" RSSI=");
      log_->println(WiFi.RSSI());
    }
  }
}

bool WifiManager::isConnected() const {
  return connected_;
}

String WifiManager::localIp() const {
  return WiFi.localIP().toString();
}

}  // namespace app
