#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

namespace espnow {

// Broadcast address — all peers receive.  Replace with a specific MAC to
// restrict delivery to a single companion board later.
constexpr uint8_t kBroadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Wi-Fi channel both boards must agree on.
constexpr uint8_t kChannel = 1;

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
  peer.channel = kChannel;
  peer.encrypt = false;

  return esp_now_add_peer(&peer) == ESP_OK;
}

}  // namespace espnow
