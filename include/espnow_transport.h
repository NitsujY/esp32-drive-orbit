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

// Initialise ESP-NOW without altering the Wi-Fi mode or connection.
// Use this on devices (e.g. the headless gateway) that manage their own
// Wi-Fi stack and must not disconnect to initialise the transport.
// Returns true on success.
inline bool beginGatewayTransport() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  return true;
}

// Register the broadcast peer so esp_now_send can target it.
// channel=0 means "follow the current radio channel", which is correct whether
// Wi-Fi is associated to an AP or in standalone mode.
inline bool addBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, kBroadcastAddress, 6);
  peer.channel = 0;  // Follow the current radio / AP channel.
  peer.encrypt = false;

  return esp_now_add_peer(&peer) == ESP_OK;
}

}  // namespace espnow
