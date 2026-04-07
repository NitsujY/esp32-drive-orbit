#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>

#include "wifi_config.h"

namespace app {

namespace {

constexpr uint32_t kRetryIntervalMs = 15000;
constexpr uint32_t kConnectTimeoutMs = 12000;
constexpr uint32_t kLogIntervalMs = 30000;
constexpr int kScanResultLogLimit = 5;
constexpr char kWifiHostname[] = "carconsole";

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
  stopped_ = false;

  // Configure NTP timezone.
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  startConnection(millis(), "Connecting to");
  ntp_synced_ = false;
}

void WifiManager::poll(uint32_t now_ms) {
  if (stopped_) {
    connected_ = false;
    return;
  }

  const wl_status_t status = WiFi.status();
  const bool was_connected = connected_;
  connected_ = (status == WL_CONNECTED);

  if (status != last_status_) {
    log_->print("[WIFI] Status -> ");
    log_->println(wifiStatusLabel(status));

    if (status == WL_NO_SSID_AVAIL) {
      log_->println("[WIFI] Configured SSID not found. ESP32 hotspots must be exposed on 2.4 GHz.");
    } else if (status == WL_CONNECT_FAILED) {
      log_->println("[WIFI] Association failed. Recheck hotspot password and security mode.");
    }

    last_status_ = status;
  }

  if (connected_ && !was_connected) {
    log_->print("[WIFI] Connected. IP: ");
    log_->print(WiFi.localIP());
    log_->print(" RSSI=");
    log_->print(WiFi.RSSI());
    log_->print(" CH=");
    log_->println(WiFi.channel());
  }

  if (!connected_ && was_connected) {
    log_->println("[WIFI] Disconnected from hotspot");
    ntp_synced_ = false;
  }

  if (!connected_ && attempted_ && !connect_timeout_logged_ &&
      (now_ms - last_attempt_ms_ >= kConnectTimeoutMs)) {
    log_->print("[WIFI] Connect timeout. Status=");
    log_->println(wifiStatusLabel(status));
    logScanDiagnostics();
    connect_timeout_logged_ = true;
  }

  // Retry connection periodically.
  if (!connected_ && (now_ms - last_attempt_ms_ >= kRetryIntervalMs)) {
    log_->print("[WIFI] Last status before retry: ");
    log_->println(wifiStatusLabel(status));
    startConnection(now_ms, "Retrying connection to");
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

bool WifiManager::isTimeSynced() const {
  return ntp_synced_;
}

String WifiManager::localIp() const {
  return WiFi.localIP().toString();
}

const char *WifiManager::hostname() const {
  return kWifiHostname;
}

void WifiManager::stop(const char *reason) {
  if (stopped_) {
    return;
  }

  if (log_ != nullptr) {
    log_->print("[WIFI] ");
    log_->println(reason);
  }

  WiFi.disconnect(true, false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);

  connected_ = false;
  attempted_ = true;
  stopped_ = true;
  last_status_ = WL_DISCONNECTED;
}

void WifiManager::startConnection(uint32_t now_ms, const char *reason) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(kWifiHostname);
  // BLE is active for ELM327, so ESP32 requires Wi-Fi modem sleep to remain enabled.
  WiFi.setSleep(true);
  WiFi.disconnect(false, true);
  delay(150);

  if (log_ != nullptr) {
    log_->print("[WIFI] ");
    log_->print(reason);
    log_->println(WIFI_SSID);
    logScanDiagnostics();
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  attempted_ = true;
  connected_ = false;
  stopped_ = false;
  last_attempt_ms_ = now_ms;
  last_status_ = WiFi.status();
  connect_timeout_logged_ = false;
}

void WifiManager::logScanDiagnostics() {
  if (log_ == nullptr) {
    return;
  }

  log_->println("[WIFI] Scanning visible networks...");
  const int network_count = WiFi.scanNetworks();
  if (network_count < 0) {
    log_->println("[WIFI] Scan failed");
    return;
  }

  int found_channel = 0;
  int32_t found_rssi = -127;
  bool found_ssid = false;

  for (int index = 0; index < network_count; ++index) {
    if (WiFi.SSID(index) == WIFI_SSID) {
      found_ssid = true;
      if (WiFi.RSSI(index) >= found_rssi) {
        found_rssi = WiFi.RSSI(index);
        found_channel = WiFi.channel(index);
      }
    }
  }

  if (found_ssid) {
    log_->print("[WIFI] Target SSID visible. RSSI=");
    log_->print(found_rssi);
    log_->print(" channel=");
    log_->println(found_channel);
  } else {
    log_->println("[WIFI] Target SSID not visible. If this is a phone hotspot, force 2.4 GHz mode.");
    const int log_count = min(network_count, kScanResultLogLimit);
    for (int index = 0; index < log_count; ++index) {
      log_->print("[WIFI]   saw ");
      log_->print(WiFi.SSID(index));
      log_->print(" RSSI=");
      log_->print(WiFi.RSSI(index));
      log_->print(" channel=");
      log_->println(WiFi.channel(index));
    }
  }

  WiFi.scanDelete();
}

}  // namespace app
