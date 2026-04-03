#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

namespace espnow {

// Broadcast address — all peers receive.  Replace with a specific MAC to
// restrict delivery to a single companion board later.
constexpr uint8_t kBroadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Fallback Wi-Fi channel for standalone ESP-NOW use before a station channel exists.
constexpr uint8_t kFallbackChannel = 1;

// Initialise Wi-Fi in STA mode and start ESP-NOW.
// Returns true on success.
inline bool beginTransport() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    return false;
  }

  return true;
}

// Register the broadcast peer so esp_now_send can target it.
inline bool addBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, kBroadcastAddress, 6);
  // Use the current home channel when Wi-Fi is connected; otherwise fall back
  // to the default ESP-NOW review channel for standalone operation.
  const uint8_t current_channel = static_cast<uint8_t>(WiFi.channel());
  peer.channel = current_channel == 0 ? kFallbackChannel : 0;
  peer.encrypt = false;

  return esp_now_add_peer(&peer) == ESP_OK;
}

}  // namespace espnow
