#pragma once

#include <Arduino.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "car_telemetry_store.h"

namespace app {

class DashboardWebServer {
 public:
  explicit DashboardWebServer(const CarTelemetryStore &telemetry_store);

  void begin(Print &log_output);
  void poll(uint32_t now_ms, bool wifi_connected);
  bool hasHostedUi() const;
  bool usingEmbeddedFallback() const;
  uint8_t connectedClientCount() const;

 private:
  void handleSocketEvent(AsyncWebSocket *server,
                         AsyncWebSocketClient *client,
                         AwsEventType type,
                         void *arg,
                         uint8_t *data,
                         size_t len);
  void sendSnapshot(AsyncWebSocketClient *client, const telemetry::CarTelemetry &snapshot);
  void broadcastSnapshot(uint32_t now_ms);
  void sendAsset(AsyncWebServerRequest *request,
                 const char *path,
                 const char *content_type,
                 bool cache_forever);
  void sendEmbeddedAsset(AsyncWebServerRequest *request,
                         const char *content_type,
                         const char *content);
  bool startMdns();

  const CarTelemetryStore &telemetry_store_;
  AsyncWebServer server_;
  AsyncWebSocket socket_;
  Print *log_output_ = nullptr;
  bool littlefs_ready_ = false;
  bool hosted_ui_available_ = false;
  bool server_started_ = false;
  bool mdns_started_ = false;
  bool last_wifi_connected_ = false;
  uint32_t last_broadcast_ms_ = 0;
};

}  // namespace app