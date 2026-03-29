#pragma once

#include <Arduino.h>

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
};

}  // namespace app
