#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace app {

class Elm327Client {
 public:
  enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Live = 2,
  };

  enum class InitCommand : uint8_t {
    EchoOff = 0,
    LinefeedsOff = 1,
    SpacesOff = 2,
    HeadersOff = 3,
    SetProtocol = 4,
    BroadcastHeader = 5,
    Complete = 6,
  };

  enum class QueryKind : uint8_t {
    None = 0,
    Rpm = 1,
    Speed = 2,
    Coolant = 3,
    Voltage = 4,
    FuelStandard = 5,
    FuelToyota = 6,
  };

  enum class FuelSequenceStep : uint8_t {
    Idle = 0,
    EnableHeaders = 1,
    SetToyotaHeader = 2,
    QueryToyotaFuel = 3,
    RestoreBroadcastHeader = 4,
    DisableHeaders = 5,
  };

  void begin(Stream &log_output);
  void poll(uint32_t now_ms, telemetry::DashboardTelemetry &telemetry);
  bool hasFreshTelemetry(uint32_t now_ms) const;
  bool connected() const;
  ConnectionState connectionState() const;
  const char *statusText() const;

 private:
  static void connectionTask(void *param);
  void updateConnection(uint32_t now_ms);
  void updateInitialization(uint32_t now_ms);
  void updatePolling(uint32_t now_ms);
  void processIncoming(uint32_t now_ms, telemetry::DashboardTelemetry &telemetry);
  void handleLine(const char *line, uint32_t now_ms, telemetry::DashboardTelemetry &telemetry);
  void sendCommand(const char *command);
  void advanceQuery();
  void startFuelSequence();
  void updateFuelSequence(uint32_t now_ms);
  bool maybeStartFuelPolling(uint32_t now_ms);
  static bool parseHexByte(const char *text, uint8_t &value);
  static bool parseVoltageMv(const char *text, uint16_t &millivolts);
  static bool parseToyotaFuelByte(const char *text, uint8_t &value);

  Stream *log_output_ = nullptr;
  bool started_ = false;
  bool connected_ = false;
  bool connect_task_running_ = false;
  bool standard_fuel_unsupported_ = false;
  uint32_t last_connect_attempt_ms_ = 0;
  uint32_t last_command_ms_ = 0;
  uint32_t last_fuel_request_ms_ = 0;
  uint32_t last_fuel_update_ms_ = 0;
  uint32_t last_live_update_ms_ = 0;
  uint32_t fuel_unknown_since_ms_ = 0;
  uint32_t last_speed_update_ms_ = 0;
  uint32_t last_rpm_update_ms_ = 0;
  uint32_t elm_busy_until_ms_ = 0;
  size_t next_target_index_ = 0;
  const char *connected_target_ = nullptr;
  ConnectionState connection_state_ = ConnectionState::Disconnected;
  InitCommand init_command_ = InitCommand::EchoOff;
  QueryKind active_query_ = QueryKind::None;
  QueryKind next_query_ = QueryKind::Rpm;
  FuelSequenceStep fuel_sequence_step_ = FuelSequenceStep::Idle;
  size_t line_length_ = 0;
  char line_buffer_[96] = {0};
  float smoothed_fuel_pct_ = -1.0f;  // <0 means uninitialized
};

}  // namespace app