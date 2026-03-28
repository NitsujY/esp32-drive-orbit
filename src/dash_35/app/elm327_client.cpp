#include "elm327_client.h"

#include "vehicle_profiles/vehicle_profile.h"

#include <BLEDevice.h>

#include <ctype.h>
#include <map>
#include <stdlib.h>
#include <string.h>

namespace app {

namespace {

constexpr char kBluetoothClientName[] = "DriveOrbitDash";
constexpr uint32_t kConnectRetryMs = 6000;
constexpr uint32_t kInitCommandDelayMs = 320;
constexpr uint32_t kQueryIntervalMs = 180;
constexpr uint32_t kFuelQueryIntervalMs = 5000;
constexpr uint32_t kToyotaFuelFallbackDelayMs = 12000;
constexpr uint32_t kFreshTelemetryTimeoutMs = 1800;
constexpr uint32_t kBleScanSeconds = 4;
constexpr size_t kBleRxBufferSize = 1024;
constexpr char kNusServiceUuid[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kNusTxUuid[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kNusRxUuid[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

BLEClient *ble_client = nullptr;
BLEScan *ble_scan = nullptr;
BLERemoteCharacteristic *ble_tx_characteristic = nullptr;
BLERemoteCharacteristic *ble_rx_characteristic = nullptr;
bool ble_ready = false;
char ble_rx_buffer[kBleRxBufferSize] = {0};
size_t ble_rx_head = 0;
size_t ble_rx_tail = 0;
portMUX_TYPE ble_rx_mux = portMUX_INITIALIZER_UNLOCKED;

bool containsIgnoreCase(const std::string &candidate, const char *target_name) {
  if (target_name == nullptr || candidate.empty()) {
    return false;
  }

  const size_t target_length = strlen(target_name);
  if (target_length == 0 || candidate.size() < target_length) {
    return false;
  }

  for (size_t start = 0; start + target_length <= candidate.size(); ++start) {
    bool match = true;
    for (size_t i = 0; i < target_length; ++i) {
      if (toupper(static_cast<unsigned char>(candidate[start + i])) !=
          toupper(static_cast<unsigned char>(target_name[i]))) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }

  return false;
}

bool namesMatch(const std::string &candidate, const char *target_name) {
  if (target_name == nullptr || candidate.empty()) {
    return false;
  }

  if (containsIgnoreCase(candidate, target_name)) {
    return true;
  }

  return containsIgnoreCase(std::string(target_name), candidate.c_str());
}

void enqueueBleBytes(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0) {
    return;
  }

  portENTER_CRITICAL(&ble_rx_mux);
  for (size_t index = 0; index < length; ++index) {
    const size_t next_head = (ble_rx_head + 1U) % kBleRxBufferSize;
    if (next_head == ble_rx_tail) {
      ble_rx_tail = (ble_rx_tail + 1U) % kBleRxBufferSize;
    }
    ble_rx_buffer[ble_rx_head] = static_cast<char>(data[index]);
    ble_rx_head = next_head;
  }
  portEXIT_CRITICAL(&ble_rx_mux);
}

bool dequeueBleByte(char &out) {
  bool has_byte = false;
  portENTER_CRITICAL(&ble_rx_mux);
  if (ble_rx_tail != ble_rx_head) {
    out = ble_rx_buffer[ble_rx_tail];
    ble_rx_tail = (ble_rx_tail + 1U) % kBleRxBufferSize;
    has_byte = true;
  }
  portEXIT_CRITICAL(&ble_rx_mux);
  return has_byte;
}

void bleNotifyCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  enqueueBleBytes(data, length);
}

void resetBleTransport() {
  ble_tx_characteristic = nullptr;
  ble_rx_characteristic = nullptr;
  portENTER_CRITICAL(&ble_rx_mux);
  ble_rx_head = 0;
  ble_rx_tail = 0;
  portEXIT_CRITICAL(&ble_rx_mux);
}

bool discoverBleCharacteristics(Stream *log_output) {
  ble_tx_characteristic = nullptr;
  ble_rx_characteristic = nullptr;

  const char *preferred_service_uuid = vehicle_profiles::bleServiceUuid();
  const char *preferred_tx_uuid = vehicle_profiles::bleTxUuid();
  const char *preferred_rx_uuid = vehicle_profiles::bleRxUuid();

  // Phase 1: Try the vehicle-profile service UUID to trigger GATT discovery.
  BLERemoteService *preferred_service = nullptr;
  if (preferred_service_uuid != nullptr) {
    preferred_service = ble_client->getService(BLEUUID(preferred_service_uuid));
    if (preferred_service != nullptr) {
      if (log_output != nullptr) {
        log_output->print("[ELM327] Found preferred service: ");
        log_output->println(preferred_service_uuid);
      }
      if (preferred_tx_uuid != nullptr) {
        ble_tx_characteristic = preferred_service->getCharacteristic(BLEUUID(preferred_tx_uuid));
      }
      if (preferred_rx_uuid != nullptr) {
        ble_rx_characteristic = preferred_service->getCharacteristic(BLEUUID(preferred_rx_uuid));
      }
    }
  }

  // Phase 2: If preferred service didn't yield both chars, try NUS to trigger
  // full service discovery (getService triggers GATT discovery on the ESP32
  // BLE stack; getServices only returns already-discovered services).
  if (ble_tx_characteristic == nullptr || ble_rx_characteristic == nullptr) {
    BLERemoteService *nus_service = ble_client->getService(BLEUUID(kNusServiceUuid));
    if (nus_service != nullptr) {
      if (log_output != nullptr) {
        log_output->println("[ELM327] Found NUS service.");
      }
      if (ble_tx_characteristic == nullptr) {
        ble_tx_characteristic = nus_service->getCharacteristic(BLEUUID(kNusTxUuid));
      }
      if (ble_rx_characteristic == nullptr) {
        ble_rx_characteristic = nus_service->getCharacteristic(BLEUUID(kNusRxUuid));
      }
    }
  }

  // Phase 3: Search all discovered services for the preferred TX/RX UUIDs.
  // Now that getService() calls above have triggered GATT discovery,
  // getServices() will return the full map.
  if ((ble_tx_characteristic == nullptr && preferred_tx_uuid != nullptr) ||
      (ble_rx_characteristic == nullptr && preferred_rx_uuid != nullptr)) {
    std::map<std::string, BLERemoteService *> *services = ble_client->getServices();
    if (services != nullptr) {
      for (auto &service_entry : *services) {
        BLERemoteService *service = service_entry.second;
        if (service == nullptr) {
          continue;
        }
        if (ble_tx_characteristic == nullptr && preferred_tx_uuid != nullptr) {
          ble_tx_characteristic = service->getCharacteristic(BLEUUID(preferred_tx_uuid));
        }
        if (ble_rx_characteristic == nullptr && preferred_rx_uuid != nullptr) {
          ble_rx_characteristic = service->getCharacteristic(BLEUUID(preferred_rx_uuid));
        }
        if (ble_tx_characteristic != nullptr && ble_rx_characteristic != nullptr) {
          break;
        }
      }
    }
  }

  // Phase 4: Last resort — find any writable + notifiable characteristics.
  if (ble_tx_characteristic == nullptr || ble_rx_characteristic == nullptr) {
    std::map<std::string, BLERemoteService *> *services = ble_client->getServices();
    if (services != nullptr) {
      for (auto &service_entry : *services) {
        BLERemoteService *service = service_entry.second;
        if (service == nullptr) {
          continue;
        }
        std::map<std::string, BLERemoteCharacteristic *> *characteristics =
            service->getCharacteristics();
        if (characteristics == nullptr) {
          continue;
        }
        for (auto &characteristic_entry : *characteristics) {
          BLERemoteCharacteristic *characteristic = characteristic_entry.second;
          if (characteristic == nullptr) {
            continue;
          }
          if (ble_tx_characteristic == nullptr &&
              (characteristic->canWrite() || characteristic->canWriteNoResponse())) {
            ble_tx_characteristic = characteristic;
          }
          if (ble_rx_characteristic == nullptr &&
              (characteristic->canNotify() || characteristic->canIndicate())) {
            ble_rx_characteristic = characteristic;
          }
        }
      }
    }
  }

  if (ble_tx_characteristic == nullptr) {
    if (log_output != nullptr) {
      log_output->println("[ELM327] BLE write characteristic not found.");
    }
    return false;
  }

  if (ble_rx_characteristic == nullptr) {
    ble_rx_characteristic = ble_tx_characteristic;
  }

  if (log_output != nullptr) {
    log_output->print("[ELM327] TX char UUID: ");
    log_output->println(ble_tx_characteristic->getUUID().toString().c_str());
    log_output->print("[ELM327] RX char UUID: ");
    log_output->println(ble_rx_characteristic->getUUID().toString().c_str());
  }

  if (ble_rx_characteristic->canNotify() || ble_rx_characteristic->canIndicate()) {
    ble_rx_characteristic->registerForNotify(bleNotifyCallback, false);
    if (log_output != nullptr) {
      log_output->println("[ELM327] RX notifications enabled.");
    }
  }

  return true;
}

bool connectBleTarget(const char *target_name, Stream *log_output) {
  if (!ble_ready) {
    BLEDevice::init(kBluetoothClientName);
    ble_scan = BLEDevice::getScan();
    ble_scan->setActiveScan(true);
    ble_scan->setInterval(120);
    ble_scan->setWindow(80);
    ble_ready = true;
  }

  resetBleTransport();
  BLEScanResults results = ble_scan->start(kBleScanSeconds, false);
  const int count = results.getCount();
  for (int index = 0; index < count; ++index) {
    BLEAdvertisedDevice device = results.getDevice(index);
    const std::string name = device.getName();
    if (!namesMatch(name, target_name)) {
      continue;
    }

    if (log_output != nullptr) {
      log_output->print("[ELM327] BLE match: ");
      log_output->print(name.c_str());
      log_output->print(" @ ");
      log_output->println(device.getAddress().toString().c_str());
    }

    if (ble_client != nullptr) {
      if (ble_client->isConnected()) {
        ble_client->disconnect();
      }
      delete ble_client;
      ble_client = nullptr;
    }

    ble_client = BLEDevice::createClient();
    if (ble_client == nullptr) {
      return false;
    }

    if (!ble_client->connect(&device)) {
      delete ble_client;
      ble_client = nullptr;
      continue;
    }

    if (!discoverBleCharacteristics(log_output)) {
      ble_client->disconnect();
      delete ble_client;
      ble_client = nullptr;
      continue;
    }

    ble_scan->clearResults();
    return true;
  }

  ble_scan->clearResults();
  return false;
}

const char *initCommandText(Elm327Client::InitCommand command) {
  switch (command) {
    case Elm327Client::InitCommand::EchoOff:
      return "ATE0";
    case Elm327Client::InitCommand::LinefeedsOff:
      return "ATL0";
    case Elm327Client::InitCommand::SpacesOff:
      return "ATS0";
    case Elm327Client::InitCommand::HeadersOff:
      return "ATH0";
    case Elm327Client::InitCommand::SetProtocol:
      return "ATSP6";
    case Elm327Client::InitCommand::BroadcastHeader:
      return "ATSH7DF";
    case Elm327Client::InitCommand::Complete:
      return nullptr;
  }

  return nullptr;
}

const char *queryText(Elm327Client::QueryKind query) {
  switch (query) {
    case Elm327Client::QueryKind::Rpm:
      return "010C";
    case Elm327Client::QueryKind::Speed:
      return "010D";
    case Elm327Client::QueryKind::Coolant:
      return "0105";
    case Elm327Client::QueryKind::Voltage:
      return "ATRV";
    case Elm327Client::QueryKind::FuelStandard:
      return "012F";
    case Elm327Client::QueryKind::FuelToyota:
      return "2129";
    case Elm327Client::QueryKind::None:
      return nullptr;
  }

  return nullptr;
}

char *trimAscii(char *text) {
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }

  char *end = text + strlen(text);
  while (end > text && isspace(static_cast<unsigned char>(*(end - 1)))) {
    --end;
  }
  *end = '\0';
  return text;
}

}  // namespace

void Elm327Client::begin(Stream &log_output) {
  log_output_ = &log_output;
  if (started_) {
    return;
  }

  started_ = true;
  if (log_output_) {
    log_output_->println("[ELM327] BLE client ready.");
    log_output_->print("[ELM327] Vehicle profile: ");
    log_output_->println(vehicle_profiles::activeProfile().display_name);
  }
}

void Elm327Client::poll(uint32_t now_ms, telemetry::DashboardTelemetry &telemetry) {
  if (!started_) {
    return;
  }

  updateConnection(now_ms);
  processIncoming(now_ms, telemetry);

  if (!connected_) {
    return;
  }

  if (init_command_ != InitCommand::Complete) {
    updateInitialization(now_ms);
    return;
  }

  // Zero stale gauges when the ECU stops responding (ignition off / adapter idle).
  // Matches the old project's timeout behavior.
  constexpr uint32_t kStaleMotionTimeoutMs = 2000;
  constexpr uint32_t kStaleFuelTimeoutMs = 15000;

  if (last_speed_update_ms_ != 0 && now_ms - last_speed_update_ms_ > kStaleMotionTimeoutMs) {
    telemetry.speed_kph = 0;
  }
  if (last_rpm_update_ms_ != 0 && now_ms - last_rpm_update_ms_ > kStaleMotionTimeoutMs) {
    telemetry.rpm = 0;
  }
  if (last_fuel_update_ms_ != 0 && now_ms - last_fuel_update_ms_ > kStaleFuelTimeoutMs) {
    telemetry.fuel_level_pct = 0;
  }

  updatePolling(now_ms);
}

bool Elm327Client::hasFreshTelemetry(uint32_t now_ms) const {
  return connected_ && init_command_ == InitCommand::Complete &&
         last_live_update_ms_ != 0 && now_ms - last_live_update_ms_ <= kFreshTelemetryTimeoutMs;
}

bool Elm327Client::connected() const {
  return connected_;
}

Elm327Client::ConnectionState Elm327Client::connectionState() const {
  return connection_state_;
}

const char *Elm327Client::statusText() const {
  switch (connection_state_) {
    case ConnectionState::Disconnected:
      if (!started_) {
        return "disabled";
      }
      return "disconnected";
    case ConnectionState::Connecting:
      return "connecting";
    case ConnectionState::Live:
      return "live";
  }

  if (!started_) {
    return "disabled";
  }

  return "disconnected";
}

namespace {

struct ConnectTaskParams {
  Elm327Client *client;
  const char *target_name;
  Stream *log_output;
};

ConnectTaskParams connect_task_params;
volatile bool connect_task_result = false;
volatile bool connect_task_done = false;

void connectTaskFunc(void *param) {
  auto *p = static_cast<ConnectTaskParams *>(param);
  connect_task_result = connectBleTarget(p->target_name, p->log_output);
  connect_task_done = true;
  vTaskDelete(nullptr);
}

}  // namespace

void Elm327Client::connectionTask(void *param) {
  (void)param;
}

void Elm327Client::updateConnection(uint32_t now_ms) {
  if (connected_) {
    if (ble_client == nullptr || !ble_client->isConnected()) {
      connected_ = false;
      standard_fuel_unsupported_ = false;
      connection_state_ = ConnectionState::Disconnected;
      fuel_sequence_step_ = FuelSequenceStep::Idle;
      init_command_ = InitCommand::EchoOff;
      active_query_ = QueryKind::None;
      last_fuel_request_ms_ = 0;
      last_fuel_update_ms_ = 0;
      fuel_unknown_since_ms_ = 0;
      last_speed_update_ms_ = 0;
      last_rpm_update_ms_ = 0;
      elm_busy_until_ms_ = 0;
      line_length_ = 0;
      resetBleTransport();
      if (log_output_) {
        log_output_->println("[ELM327] Adapter disconnected.");
      }
    }
    return;
  }

  // Check if background connect task finished
  if (connect_task_running_) {
    if (!connect_task_done) {
      return;  // still scanning, don't block
    }
    connect_task_running_ = false;
    if (connect_task_result) {
      connected_ = true;
      connected_target_ = connect_task_params.target_name;
      standard_fuel_unsupported_ = false;
      connection_state_ = ConnectionState::Connecting;
      fuel_sequence_step_ = FuelSequenceStep::Idle;
      init_command_ = InitCommand::EchoOff;
      active_query_ = QueryKind::None;
      last_command_ms_ = 0;
      last_fuel_request_ms_ = 0;
      last_fuel_update_ms_ = 0;
      fuel_unknown_since_ms_ = 0;
      last_speed_update_ms_ = 0;
      last_rpm_update_ms_ = 0;
      elm_busy_until_ms_ = 0;
      line_length_ = 0;
      if (log_output_) {
        log_output_->println("[ELM327] Connected.");
        log_output_->print("[ELM327] Target: ");
        log_output_->println(connected_target_);
      }
    } else {
      connection_state_ = ConnectionState::Disconnected;
    }
    return;
  }

  if (now_ms - last_connect_attempt_ms_ < kConnectRetryMs) {
    return;
  }

  const size_t target_count = vehicle_profiles::bluetoothTargetCount();
  if (target_count == 0) {
    return;
  }

  const char *target_name = vehicle_profiles::bluetoothTargetAt(next_target_index_);
  next_target_index_ = (next_target_index_ + 1) % target_count;
  if (target_name == nullptr) {
    return;
  }

  last_connect_attempt_ms_ = now_ms;
  connection_state_ = ConnectionState::Connecting;
  if (log_output_) {
    log_output_->print("[ELM327] Connecting to ");
    log_output_->println(target_name);
  }

  // Launch background task for blocking BLE scan + connect
  connect_task_params = {this, target_name, log_output_};
  connect_task_done = false;
  connect_task_result = false;
  connect_task_running_ = true;
  xTaskCreatePinnedToCore(connectTaskFunc, "ble_conn", 4096, &connect_task_params, 1, nullptr, 0);
}

void Elm327Client::updateInitialization(uint32_t now_ms) {
  if (now_ms - last_command_ms_ < kInitCommandDelayMs) {
    return;
  }

  const char *command = initCommandText(init_command_);
  if (command == nullptr) {
    return;
  }

  sendCommand(command);
  last_command_ms_ = now_ms;

  const auto next = static_cast<InitCommand>(static_cast<uint8_t>(init_command_) + 1);
  init_command_ = next;
  if (next == InitCommand::Complete) {
    connection_state_ = ConnectionState::Live;
    if (log_output_) {
      log_output_->println("[ELM327] Initialization complete.");
    }
  }
}

void Elm327Client::updatePolling(uint32_t now_ms) {
  if (active_query_ != QueryKind::None) {
    return;
  }

  if (fuel_sequence_step_ != FuelSequenceStep::Idle) {
    updateFuelSequence(now_ms);
    return;
  }

  if (now_ms - last_command_ms_ < kQueryIntervalMs) {
    return;
  }

  // Respect adapter busy window (BUS INIT / SEARCHING / STOPPED).
  if (elm_busy_until_ms_ != 0 &&
      static_cast<int32_t>(now_ms - elm_busy_until_ms_) < 0) {
    return;
  }

  if (maybeStartFuelPolling(now_ms)) {
    return;
  }

  const char *command = queryText(next_query_);
  if (command == nullptr) {
    return;
  }

  active_query_ = next_query_;
  advanceQuery();
  sendCommand(command);
  last_command_ms_ = now_ms;
}

void Elm327Client::processIncoming(uint32_t now_ms, telemetry::DashboardTelemetry &telemetry) {
  char ch = 0;
  while (dequeueBleByte(ch)) {
    if (ch == '\r' || ch == '\n' || ch == '>') {
      if (line_length_ > 0) {
        line_buffer_[line_length_] = '\0';
        handleLine(line_buffer_, now_ms, telemetry);
        line_length_ = 0;
      }
      continue;
    }

    if (line_length_ + 1 < sizeof(line_buffer_)) {
      line_buffer_[line_length_++] = ch;
    }
  }
}

void Elm327Client::handleLine(const char *line, uint32_t now_ms, telemetry::DashboardTelemetry &telemetry) {
  char scratch[sizeof(line_buffer_)] = {0};
  strncpy(scratch, line, sizeof(scratch) - 1);
  char *trimmed = trimAscii(scratch);
  if (*trimmed == '\0') {
    return;
  }

  if (strcmp(trimmed, "OK") == 0 || strcmp(trimmed, "SEARCHING...") == 0 ||
      strstr(trimmed, "ELM327") != nullptr) {
    if (strstr(trimmed, "SEARCHING") != nullptr || strstr(trimmed, "BUS INIT") != nullptr ||
        strstr(trimmed, "STOPPED") != nullptr) {
      const uint32_t until = now_ms + 2500U;
      if (elm_busy_until_ms_ == 0 || static_cast<int32_t>(until - elm_busy_until_ms_) > 0) {
        elm_busy_until_ms_ = until;
      }
    }
    return;
  }

  if (strcmp(trimmed, "NO DATA") == 0) {
    if (active_query_ == QueryKind::FuelStandard) {
      standard_fuel_unsupported_ = true;
      if (fuel_unknown_since_ms_ == 0) {
        fuel_unknown_since_ms_ = now_ms;
      }
      if (log_output_) {
        log_output_->println("[ELM327] Standard fuel PID 012F returned NO DATA.");
      }
    }
    active_query_ = QueryKind::None;
    return;
  }

  switch (active_query_) {
    case QueryKind::Rpm: {
      uint8_t a = 0;
      uint8_t b = 0;
      if (sscanf(trimmed, "41 0C %2hhx %2hhx", &a, &b) == 2 ||
          sscanf(trimmed, "410C%2hhx%2hhx", &a, &b) == 2) {
        telemetry.rpm = static_cast<int16_t>(((static_cast<uint16_t>(a) << 8) | b) / 4U);
        last_live_update_ms_ = now_ms;
        last_rpm_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::Speed: {
      uint8_t value = 0;
      if (sscanf(trimmed, "41 0D %2hhx", &value) == 1 ||
          sscanf(trimmed, "410D%2hhx", &value) == 1) {
        telemetry.speed_kph = vehicle_profiles::applySpeedOffset(static_cast<int16_t>(value));
        last_live_update_ms_ = now_ms;
        last_speed_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::Coolant: {
      uint8_t value = 0;
      if (sscanf(trimmed, "41 05 %2hhx", &value) == 1 ||
          sscanf(trimmed, "4105%2hhx", &value) == 1) {
        telemetry.coolant_temp_c = static_cast<int16_t>(value) - 40;
        last_live_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::FuelStandard: {
      uint8_t value = 0;
      if (sscanf(trimmed, "41 2F %2hhx", &value) == 1 ||
          sscanf(trimmed, "412F%2hhx", &value) == 1) {
      const int scaled_pct =
        (((static_cast<int>(value) * 100) / 255) *
         static_cast<int>(vehicle_profiles::activeProfile().fuel_scale_percent)) /
        100;
      telemetry.fuel_level_pct = static_cast<uint8_t>(constrain(scaled_pct, 0, 100));
        standard_fuel_unsupported_ = false;
        last_fuel_update_ms_ = now_ms;
        fuel_unknown_since_ms_ = 0;
        last_live_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::FuelToyota: {
      uint8_t value = 0;
      if (parseToyotaFuelByte(trimmed, value)) {
        const float liters = static_cast<float>(value) / 2.0f +
                             vehicle_profiles::activeProfile().toyota_fuel_offset_liters;
        const auto &profile = vehicle_profiles::activeProfile();
        const float scaled_pct = constrain(
            (liters * 100.0f / profile.fuel_tank_liters) *
                (static_cast<float>(profile.fuel_scale_percent) / 100.0f),
            0.0f, 100.0f);
        telemetry.fuel_level_pct = static_cast<uint8_t>(lroundf(scaled_pct));
        last_fuel_update_ms_ = now_ms;
        fuel_unknown_since_ms_ = 0;
        last_live_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::Voltage: {
      uint16_t millivolts = 0;
      if (parseVoltageMv(trimmed, millivolts)) {
        telemetry.battery_mv = millivolts;
        last_live_update_ms_ = now_ms;
      }
      break;
    }
    case QueryKind::None:
      break;
  }

  if (telemetry.rpm <= 0 && telemetry.speed_kph > 0 && telemetry.speed_kph < 5) {
    telemetry.speed_kph = 0;
  }

  active_query_ = QueryKind::None;
}

void Elm327Client::sendCommand(const char *command) {
  if (ble_client == nullptr || !ble_client->isConnected() || ble_tx_characteristic == nullptr) {
    return;
  }

  char payload[32] = {0};
  snprintf(payload, sizeof(payload), "%s\r", command);
  const size_t payload_length = strlen(payload);
  const bool use_response = !ble_tx_characteristic->canWriteNoResponse();
  ble_tx_characteristic->writeValue(reinterpret_cast<uint8_t *>(payload), payload_length,
                                   use_response);
}

void Elm327Client::advanceQuery() {
  switch (next_query_) {
    case QueryKind::Rpm:
      next_query_ = QueryKind::Speed;
      break;
    case QueryKind::Speed:
      next_query_ = QueryKind::Coolant;
      break;
    case QueryKind::Coolant:
      next_query_ = QueryKind::Voltage;
      break;
    case QueryKind::Voltage:
      next_query_ = QueryKind::Rpm;
      break;
    case QueryKind::FuelStandard:
    case QueryKind::FuelToyota:
    case QueryKind::None:
      next_query_ = QueryKind::Rpm;
      break;
  }
}

void Elm327Client::startFuelSequence() {
  fuel_sequence_step_ = FuelSequenceStep::EnableHeaders;
}

void Elm327Client::updateFuelSequence(uint32_t now_ms) {
  if (now_ms - last_command_ms_ < kQueryIntervalMs) {
    return;
  }

  const char *command = nullptr;
  switch (fuel_sequence_step_) {
    case FuelSequenceStep::EnableHeaders:
      command = "ATH1";
      fuel_sequence_step_ = FuelSequenceStep::SetToyotaHeader;
      break;
    case FuelSequenceStep::SetToyotaHeader:
      if (vehicle_profiles::activeProfile().toyota_fuel_header == nullptr) {
        fuel_sequence_step_ = FuelSequenceStep::Idle;
        return;
      }
      if (ble_client == nullptr || !ble_client->isConnected() || ble_tx_characteristic == nullptr) {
        fuel_sequence_step_ = FuelSequenceStep::Idle;
        return;
      }
      {
        char header_command[16] = {0};
        snprintf(header_command, sizeof(header_command), "ATSH%s",
                 vehicle_profiles::activeProfile().toyota_fuel_header);
        sendCommand(header_command);
      }
      last_command_ms_ = now_ms;
      fuel_sequence_step_ = FuelSequenceStep::QueryToyotaFuel;
      return;
    case FuelSequenceStep::QueryToyotaFuel:
      command = "2129";
      active_query_ = QueryKind::FuelToyota;
      fuel_sequence_step_ = FuelSequenceStep::RestoreBroadcastHeader;
      break;
    case FuelSequenceStep::RestoreBroadcastHeader:
      command = "ATSH7DF";
      fuel_sequence_step_ = FuelSequenceStep::DisableHeaders;
      break;
    case FuelSequenceStep::DisableHeaders:
      command = "ATH0";
      fuel_sequence_step_ = FuelSequenceStep::Idle;
      break;
    case FuelSequenceStep::Idle:
      return;
  }

  if (command != nullptr) {
    sendCommand(command);
    last_command_ms_ = now_ms;
  }
}

bool Elm327Client::maybeStartFuelPolling(uint32_t now_ms) {
  if (last_fuel_request_ms_ != 0 && now_ms - last_fuel_request_ms_ < kFuelQueryIntervalMs) {
    return false;
  }

  const auto &profile = vehicle_profiles::activeProfile();
  const bool use_toyota_fuel =
      profile.fuel_source == vehicle_profiles::FuelSource::Toyota2129 ||
      (profile.fuel_source == vehicle_profiles::FuelSource::Auto &&
       (standard_fuel_unsupported_ ||
        (fuel_unknown_since_ms_ != 0 && now_ms - fuel_unknown_since_ms_ >= kToyotaFuelFallbackDelayMs)));

  last_fuel_request_ms_ = now_ms;

  if (use_toyota_fuel) {
    startFuelSequence();
    updateFuelSequence(now_ms);
    return true;
  }

  active_query_ = QueryKind::FuelStandard;
  sendCommand("012F");
  last_command_ms_ = now_ms;
  return true;
}

bool Elm327Client::parseHexByte(const char *text, uint8_t &value) {
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 16);
  if (end == text || parsed > 0xFFUL) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

bool Elm327Client::parseToyotaFuelByte(const char *text, uint8_t &value) {
  return sscanf(text, "61 29 %2hhx", &value) == 1 || sscanf(text, "6129%2hhx", &value) == 1 ||
         sscanf(text, "7C8 03 61 29 %2hhx", &value) == 1 ||
         sscanf(text, "7c8 03 61 29 %2hhx", &value) == 1 ||
         sscanf(text, "7C8036129%2hhx", &value) == 1 || sscanf(text, "7c8036129%2hhx", &value) == 1;
}

bool Elm327Client::parseVoltageMv(const char *text, uint16_t &millivolts) {
  char *end = nullptr;
  const float volts = strtof(text, &end);
  if (end == text) {
    return false;
  }

  while (*end != '\0' && isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }

  if (*end != 'V' && *end != 'v' && *end != '\0') {
    return false;
  }

  millivolts = static_cast<uint16_t>(volts * 1000.0f);
  return true;
}

}  // namespace app