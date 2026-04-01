#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>

#include "wifi_config.h"

namespace app {

namespace {

constexpr uint32_t kRetryIntervalMs = 60000;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kLogIntervalMs = 30000;

const char *wifiStatusLabel(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD:
      return "NO_SHIELD";
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "AUTH_FAIL";
    case WL_CONNECTION_LOST:
      return "LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

void WifiManager::begin(Print &log) {
  log_ = &log;

  // ESP-NOW requires STA mode, which is already set by espnow::beginTransport().
  // We just need to start the connection — WiFi.begin() in STA mode is additive.
  log_->print("[WIFI] Connecting to ");
  log_->println(WIFI_SSID);

  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Configure NTP timezone.
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  attempted_ = true;
  last_attempt_ms_ = millis();
  last_status_ = WiFi.status();
  connect_timeout_logged_ = false;
  ntp_synced_ = false;
}

void WifiManager::poll(uint32_t now_ms) {
  const wl_status_t status = WiFi.status();
  const bool was_connected = connected_;
  connected_ = (status == WL_CONNECTED);

  if (status != last_status_) {
    log_->print("[WIFI] Status -> ");
    log_->println(wifiStatusLabel(status));
    last_status_ = status;
  }

  if (connected_ && !was_connected) {
    log_->print("[WIFI] Connected. IP: ");
    log_->println(WiFi.localIP());
  }

  if (!connected_ && was_connected) {
    log_->println("[WIFI] Disconnected from hotspot");
    ntp_synced_ = false;
  }

  if (!connected_ && attempted_ && !connect_timeout_logged_ &&
      (now_ms - last_attempt_ms_ >= kConnectTimeoutMs)) {
    log_->print("[WIFI] Connect timeout. Status=");
    log_->println(wifiStatusLabel(status));
    connect_timeout_logged_ = true;
  }

  // Retry connection periodically.
  if (!connected_ && (now_ms - last_attempt_ms_ >= kRetryIntervalMs)) {
    log_->print("[WIFI] Retrying connection to ");
    log_->print(WIFI_SSID);
    log_->print(". Last status=");
    log_->println(wifiStatusLabel(status));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    last_attempt_ms_ = now_ms;
    connect_timeout_logged_ = false;
  }

  if (connected_) {
    struct tm local_time_info {};
    const bool synced = getLocalTime(&local_time_info, 0);
    if (synced && !ntp_synced_) {
      char time_buffer[8];
      snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d", local_time_info.tm_hour,
               local_time_info.tm_min);
      log_->print("[WIFI] NTP synced: ");
      log_->println(time_buffer);
      ntp_synced_ = true;
    }
  }

  // Periodic status log.
  if (now_ms - last_log_ms_ >= kLogIntervalMs) {
    last_log_ms_ = now_ms;
    if (connected_) {
      log_->print("[WIFI] Connected, IP=");
      log_->print(WiFi.localIP());
      log_->print(" RSSI=");
      log_->print(WiFi.RSSI());
      log_->print(" NTP=");
      log_->println(ntp_synced_ ? "OK" : "PENDING");
    } else {
      log_->print("[WIFI] Waiting, status=");
      log_->println(wifiStatusLabel(status));
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
