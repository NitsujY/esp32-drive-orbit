#pragma once

#include <Arduino.h>

namespace orb {

class Elm327Obd2Client {
 public:
  enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Scanning = 1,
    Connecting = 2,
    Initializing = 3,
    Connected = 4,
  };

  void begin(Stream &log_output, uint32_t unused_baud = 38400);
  void poll(uint32_t now_ms);

  ConnectionState connectionState() const { return state_; }
  bool connected() const { return state_ == ConnectionState::Connected; }
  bool hasFreshData(uint32_t now_ms) const;

  int16_t lastSpeed() const { return last_speed_kph_; }
  int16_t lastRpm() const { return last_rpm_; }
  uint8_t lastFuelPct() const { return last_fuel_pct_; }
  uint32_t lastUpdateMs() const { return last_update_ms_; }
  const char *statusText() const;

 private:
  void startScan(uint32_t now_ms);
  void updateScanning(uint32_t now_ms);
  void updateInitialization(uint32_t now_ms);
  void updatePolling(uint32_t now_ms);
  void processIncoming(uint32_t now_ms);
  void handleLine(const char *line, uint32_t now_ms);
  void sendCommand(const char *command);
  void queryNextPid();

  static bool parseHexByte(const char *text, uint8_t &value);

  Stream *log_output_ = nullptr;
  ConnectionState state_ = ConnectionState::Disconnected;
  uint32_t last_scan_ms_ = 0;
  uint32_t last_command_ms_ = 0;
  uint32_t last_update_ms_ = 0;
  uint32_t last_init_step_ms_ = 0;

  int16_t last_speed_kph_ = 0;
  int16_t last_rpm_ = 0;
  uint8_t last_fuel_pct_ = 0;

  uint8_t init_step_ = 0;
  bool prompt_received_ = false;

  size_t line_buffer_pos_ = 0;
  char line_buffer_[64] = {0};

  uint8_t query_cycle_ = 0;
};

}  // namespace orb
