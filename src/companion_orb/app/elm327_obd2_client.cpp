#include "elm327_obd2_client.h"
#include <BLEDevice.h>

namespace orb {

namespace {

const char* kBluetoothTargets[] = {"V-LINK", "IOS-Vlink", "VLINK", "ELM327"};
constexpr size_t kTargetCount = 4;

BLEClient* ble_client = nullptr;
BLERemoteCharacteristic* tx_char = nullptr;
BLERemoteCharacteristic* rx_char = nullptr;
bool ble_ready = false;

char rx_buffer[1024];
volatile size_t rx_head = 0;
volatile size_t rx_tail = 0;

void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  for (size_t i = 0; i < length; i++) {
    size_t next_head = (rx_head + 1) % 1024;
    if (next_head != rx_tail) {
      rx_buffer[rx_head] = (char)pData[i];
      rx_head = next_head;
    }
  }
}

} // namespace

void Elm327Obd2Client::begin(Stream &log_output, uint32_t unused_baud) {
  log_output_ = &log_output;
  state_ = ConnectionState::Disconnected;
  init_step_ = 0;
  
  BLEDevice::init("CompanionOrb");
  ble_ready = true;
  log_output_->println("[OBD2] BLE Init");
}

void Elm327Obd2Client::poll(uint32_t now_ms) {
  switch (state_) {
    case ConnectionState::Disconnected:
      if (now_ms - last_scan_ms_ > 5000) {
        last_scan_ms_ = now_ms;
        startScan(now_ms);
      }
      break;
    case ConnectionState::Scanning:
      // Scan occurs in background or blocking?
      // startScan will block in this simple implementation
      break;
    case ConnectionState::Connecting:
      break;
    case ConnectionState::Initializing:
      updateInitialization(now_ms);
      break;
    case ConnectionState::Connected:
      if (ble_client && !ble_client->isConnected()) {
        state_ = ConnectionState::Disconnected;
        return;
      }
      updatePolling(now_ms);
      break;
  }
  
  processIncoming(now_ms);
}

void Elm327Obd2Client::startScan(uint32_t now_ms) {
  if (!ble_ready) return;
  state_ = ConnectionState::Scanning;
  log_output_->println("[OBD2] Scanning for ELM327...");
  
  BLEScan* ble_scan = BLEDevice::getScan();
  ble_scan->setActiveScan(true);
  BLEScanResults results = ble_scan->start(3, false);
  
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    std::string name = dev.getName();
    for (size_t j = 0; j < kTargetCount; j++) {
      if (name.find(kBluetoothTargets[j]) != std::string::npos) {
        log_output_->print("[OBD2] Found target: ");
        log_output_->println(name.c_str());
        
        if (ble_client) {
          if (ble_client->isConnected()) ble_client->disconnect();
          delete ble_client;
        }
        ble_client = BLEDevice::createClient();
        if (ble_client->connect(&dev)) {
          log_output_->println("[OBD2] Connected to GATT!");
          
          tx_char = nullptr;
          rx_char = nullptr;
          // Just grab first writable + notifiable characteristic
          std::map<std::string, BLERemoteService*>* services = ble_client->getServices();
          for (auto& s : *services) {
            std::map<std::string, BLERemoteCharacteristic*>* chars = s.second->getCharacteristics();
            for (auto& c : *chars) {
              if (!tx_char && (c.second->canWrite() || c.second->canWriteNoResponse())) {
                tx_char = c.second;
              }
              if (!rx_char && (c.second->canNotify() || c.second->canIndicate())) {
                rx_char = c.second;
              }
            }
          }
          
          if (!rx_char && tx_char && (tx_char->canNotify() || tx_char->canIndicate())) {
            rx_char = tx_char;
          }
          
          if (tx_char && rx_char) {
            rx_char->registerForNotify(notifyCallback);
            rx_head = rx_tail = 0;
            state_ = ConnectionState::Initializing;
            init_step_ = 0;
            last_init_step_ms_ = now_ms;
            ble_scan->clearResults();
            return;
          } else {
            ble_client->disconnect();
          }
        }
      }
    }
  }
  
  ble_scan->clearResults();
  state_ = ConnectionState::Disconnected;
}

bool Elm327Obd2Client::hasFreshData(uint32_t now_ms) const {
  return (now_ms - last_update_ms_) < 2000 && last_update_ms_ != 0;
}

const char *Elm327Obd2Client::statusText() const {
  switch (state_) {
    case ConnectionState::Disconnected: return "Idle";
    case ConnectionState::Scanning: return "Scanning BLE...";
    case ConnectionState::Connecting: return "Connecting...";
    case ConnectionState::Initializing: return "ELM Init...";
    case ConnectionState::Connected: return "Live";
    default: return "Unknown";
  }
}

void Elm327Obd2Client::updateInitialization(uint32_t now_ms) {
  if (now_ms - last_init_step_ms_ < 100) return;
  last_init_step_ms_ = now_ms;
  
  switch (init_step_) {
    case 0: sendCommand("ATE0\r"); break;
    case 1: sendCommand("ATL0\r"); break;
    case 2: sendCommand("ATS0\r"); break;
    case 3: sendCommand("ATH0\r"); break;
    case 4: sendCommand("ATSP6\r"); break;
    case 5:
      state_ = ConnectionState::Connected;
      query_cycle_ = 0;
      queryNextPid();
      return;
  }
  init_step_++;
}

void Elm327Obd2Client::updatePolling(uint32_t now_ms) {
  if (now_ms - last_command_ms_ >= 250 && prompt_received_) {
    prompt_received_ = false;
    queryNextPid();
  }
}

void Elm327Obd2Client::processIncoming(uint32_t now_ms) {
  while (rx_head != rx_tail) {
    char byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % 1024;
    
    if (byte == '\r' || byte == '\n') {
      if (line_buffer_pos_ > 0) {
        line_buffer_[line_buffer_pos_] = '\0';
        handleLine(line_buffer_, now_ms);
        line_buffer_pos_ = 0;
      }
    } else if (byte > 31 && byte < 127) {
      if (line_buffer_pos_ < sizeof(line_buffer_) - 1) {
        line_buffer_[line_buffer_pos_++] = byte;
      }
    }
  }
}

void Elm327Obd2Client::handleLine(const char *line, uint32_t now_ms) {
  if (strcmp(line, ">") == 0) {
    prompt_received_ = true;
    return;
  }
  if (state_ == ConnectionState::Initializing) return;
  
  if (state_ == ConnectionState::Connected && strlen(line) > 0) {
    if (line[0] == '4') {
      uint8_t pid = 0;
      if (parseHexByte(&line[2], pid)) {
        uint8_t b1=0, b2=0;
        if (pid == 0x0C && strlen(line) >= 8) {
          if (parseHexByte(&line[4], b1) && parseHexByte(&line[6], b2)) {
            last_rpm_ = ((b1 << 8) | b2) / 4;
            last_update_ms_ = now_ms;
          }
        } else if (pid == 0x0D && strlen(line) >= 6) {
          if (parseHexByte(&line[4], b1)) {
            last_speed_kph_ = b1;
            last_update_ms_ = now_ms;
          }
        } else if (pid == 0x2F && strlen(line) >= 6) {
          if (parseHexByte(&line[4], b1)) {
            last_fuel_pct_ = b1;
            last_update_ms_ = now_ms;
          }
        }
      }
    }
  }
}

void Elm327Obd2Client::sendCommand(const char *command) {
  if (!tx_char) return;
  tx_char->writeValue((uint8_t*)command, strlen(command), tx_char->canWriteNoResponse() ? false : true);
  last_command_ms_ = millis();
}

void Elm327Obd2Client::queryNextPid() {
  if (state_ != ConnectionState::Connected) return;
  switch (query_cycle_) {
    case 0: sendCommand("010C\r"); break;
    case 1: sendCommand("010D\r"); break;
    case 2: sendCommand("012F\r"); break;
  }
  query_cycle_ = (query_cycle_ + 1) % 3;
}

bool Elm327Obd2Client::parseHexByte(const char *text, uint8_t &value) {
  if (!text || strlen(text) < 2) return false;
  char hex[3] = {text[0], text[1], '\0'};
  char *endptr = nullptr;
  long result = strtol(hex, &endptr, 16);
  if (endptr == &hex[2] && result >= 0 && result <= 255) {
    value = static_cast<uint8_t>(result);
    return true;
  }
  return false;
}

}  // namespace orb

