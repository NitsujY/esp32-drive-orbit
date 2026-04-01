#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace app {

class WifiManager {
 public:
  void begin(Print &log);
  void poll(uint32_t now_ms);

  bool isConnected() const;
  String localIp() const;

 private:
  Print *log_ = nullptr;
  bool connected_ = false;
  bool attempted_ = false;
  uint32_t last_attempt_ms_ = 0;
  uint32_t last_log_ms_ = 0;
  wl_status_t last_status_ = WL_IDLE_STATUS;
  bool connect_timeout_logged_ = false;
  bool ntp_synced_ = false;
};

}  // namespace app
