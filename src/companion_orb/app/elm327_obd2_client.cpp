#include "elm327_obd2_client.h"

namespace orb {

void Elm327Obd2Client::begin(Stream &log_output, uint32_t baud_rate) {
  log_output_ = &log_output;
  state_ = ConnectionState::Initializing;
  init_step_ = 0;
  last_init_step_ms_ = millis();
  
  // For ESP32-C3, use Serial1 on pins 20 (RX) and 21 (TX)
  // Adjust based on your wiring
  elm_uart_ = new HardwareSerial(1);
  elm_uart_->begin(baud_rate, SERIAL_8N1, 20, 21);
  
  log_output_->println("[OBD2] Elm327Obd2Client starting");
}

void Elm327Obd2Client::poll(uint32_t now_ms) {
  if (state_ == ConnectionState::Initializing) {
    updateInitialization(now_ms);
  } else if (state_ == ConnectionState::Connected) {
    updatePolling(now_ms);
  }
  
  processIncoming(now_ms);
}

bool Elm327Obd2Client::hasFreshData(uint32_t now_ms) const {
  const uint32_t STALE_THRESHOLD = 2000;  // 2 seconds
  return (now_ms - last_update_ms_) < STALE_THRESHOLD && last_update_ms_ != 0;
}

const char *Elm327Obd2Client::statusText() const {
  switch (state_) {
    case ConnectionState::Disconnected:
      return "Disconnected";
    case ConnectionState::Initializing:
      return "Initializing...";
    case ConnectionState::Connected:
      return "Connected";
    default:
      return "Unknown";
  }
}

void Elm327Obd2Client::updateInitialization(uint32_t now_ms) {
  const uint32_t INIT_STEP_INTERVAL = 100;  // 100ms between init steps
  
  if (now_ms - last_init_step_ms_ < INIT_STEP_INTERVAL) {
    return;
  }
  
  last_init_step_ms_ = now_ms;
  
  // Initialization sequence
  switch (init_step_) {
    case 0:
      log_output_->println("[OBD2] Init step 0: ATE0 (echo off)");
      sendCommand("ATE0\r");
      break;
    case 1:
      log_output_->println("[OBD2] Init step 1: ATL0 (linefeeds off)");
      sendCommand("ATL0\r");
      break;
    case 2:
      log_output_->println("[OBD2] Init step 2: ATS0 (spaces off)");
      sendCommand("ATS0\r");
      break;
    case 3:
      log_output_->println("[OBD2] Init step 3: ATH0 (headers off)");
      sendCommand("ATH0\r");
      break;
    case 4:
      log_output_->println("[OBD2] Init step 4: ATSP6 (set protocol auto)");
      sendCommand("ATSP6\r");
      break;
    case 5:
      // Connection successful!
      log_output_->println("[OBD2] Initialization complete!");
      state_ = ConnectionState::Connected;
      query_cycle_ = 0;
      queryNextPid();
      return;
  }
  
  init_step_++;
}

void Elm327Obd2Client::updatePolling(uint32_t now_ms) {
  const uint32_t QUERY_INTERVAL = 200;  // 200ms between queries
  
  if (now_ms - last_command_ms_ >= QUERY_INTERVAL && prompt_received_) {
    prompt_received_ = false;
    queryNextPid();
  }
}

void Elm327Obd2Client::processIncoming(uint32_t now_ms) {
  while (elm_uart_ && elm_uart_->available()) {
    int byte = elm_uart_->read();
    
    if (byte == '\r' || byte == '\n') {
      if (line_buffer_pos_ > 0) {
        line_buffer_[line_buffer_pos_] = '\0';
        handleLine(line_buffer_, now_ms);
        line_buffer_pos_ = 0;
      }
    } else if (byte > 31 && byte < 127) {  // Printable ASCII
      if (line_buffer_pos_ < sizeof(line_buffer_) - 1) {
        line_buffer_[line_buffer_pos_++] = static_cast<char>(byte);
      }
    }
  }
}

void Elm327Obd2Client::handleLine(const char *line, uint32_t now_ms) {
  if (strlen(line) == 0) return;
  
  // Check for ELM327 prompt
  if (strcmp(line, ">") == 0) {
    prompt_received_ = true;
    return;
  }
  
  // During initialization, just look for "OK" responses
  if (state_ == ConnectionState::Initializing) {
    if (strstr(line, "OK") != nullptr) {
      log_output_->print("[OBD2] Init confirmed: ");
      log_output_->println(line);
    }
    return;
  }
  
  // OBD2 response parsing (connected state)
  if (state_ == ConnectionState::Connected && strlen(line) > 0) {
    // Parse OBD2 responses
    // Format examples:
    // 410C0F40 = RPM = 0x0F40 = 3904 RPM -> 976 rpm (divide by 4)
    // 410D78 = Speed = 0x78 = 120 km/h
    // 412F45 = Fuel = 0x45 = 69%
    
    if (line[0] == '4') {
      // Standard positive response (0x40 + 0x01 for mode 01)
      if (strlen(line) >= 4) {
        uint8_t pid = 0;
        if (parseHexByte(&line[2], pid)) {
          uint8_t byte1 = 0, byte2 = 0;
          
          if (pid == 0x0C && strlen(line) >= 8) {
            // RPM (2 bytes)
            if (parseHexByte(&line[4], byte1) && parseHexByte(&line[6], byte2)) {
              uint16_t rpm_raw = (byte1 << 8) | byte2;
              last_rpm_ = rpm_raw / 4;
              last_update_ms_ = now_ms;
              log_output_->print("[OBD2] RPM: ");
              log_output_->println(last_rpm_);
            }
          } else if (pid == 0x0D && strlen(line) >= 6) {
            // Speed (1 byte)
            if (parseHexByte(&line[4], byte1)) {
              last_speed_kph_ = byte1;
              last_update_ms_ = now_ms;
              log_output_->print("[OBD2] Speed: ");
              log_output_->println(last_speed_kph_);
            }
          } else if (pid == 0x2F && strlen(line) >= 6) {
            // Fuel level (1 byte)
            if (parseHexByte(&line[4], byte1)) {
              last_fuel_pct_ = byte1;
              log_output_->print("[OBD2] Fuel: ");
              log_output_->print(last_fuel_pct_);
              log_output_->println("%");
            }
          }
        }
      }
    } else if (strstr(line, "NO DATA") != nullptr || strstr(line, "?") != nullptr) {
      log_output_->print("[OBD2] No data response: ");
      log_output_->println(line);
    }
  }
}

void Elm327Obd2Client::sendCommand(const char *command) {
  if (!elm_uart_) return;
  
  elm_uart_->print(command);
  last_command_ms_ = millis();
  
  log_output_->print("[OBD2] TX: ");
  log_output_->println(command);
}

void Elm327Obd2Client::queryNextPid() {
  if (state_ != ConnectionState::Connected) return;
  
  // Cycle through: RPM (010C) → Speed (010D) → Fuel (012F)
  switch (query_cycle_) {
    case 0:
      sendCommand("010C\r");  // RPM
      break;
    case 1:
      sendCommand("010D\r");  // Speed
      break;
    case 2:
      sendCommand("012F\r");  // Fuel
      break;
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
