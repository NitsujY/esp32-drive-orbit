#include "dashboard_web_server.h"

#include <ESPmDNS.h>
#include <LittleFS.h>

namespace app {

namespace {

constexpr char kHostname[] = "carconsole";
constexpr char kWebSocketPath[] = "/ws";
constexpr char kApiTelemetryPath[] = "/api/telemetry";
constexpr uint32_t kBroadcastIntervalMs = 125;

constexpr char kEmbeddedIndexHtml[] PROGMEM =
  R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,viewport-fit=cover"><meta name="theme-color" content="#08111f"><meta name="application-name" content="Drive Orbit"><title>Drive Orbit</title><style>html,body{margin:0;height:100%;overflow:hidden;background:#08111f;color:#ebf8ff;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}body{display:grid;place-items:center;min-height:100vh;min-height:100dvh;padding:max(10px,env(safe-area-inset-top)) 10px max(12px,env(safe-area-inset-bottom))}main{width:min(92vw,420px);padding:20px;border:1px solid rgba(134,255,225,.18);border-radius:24px;background:rgba(7,12,24,.88)}h1{margin:0 0 8px;font-size:18px;color:#86ffe1;text-transform:uppercase;letter-spacing:.12em}.speed{font-size:92px;line-height:.8;font-weight:700}.unit{color:#86ffe1;text-transform:uppercase;letter-spacing:.14em;font-size:12px}.row{display:flex;justify-content:space-between;gap:10px;margin-top:16px}.chip{padding:10px 12px;border-radius:999px;background:rgba(255,255,255,.05);font-size:12px;color:#9cb3c3}.card{padding:12px 14px;border:1px solid rgba(134,255,225,.16);border-radius:16px;background:rgba(255,255,255,.03);flex:1;min-width:0}.label{font-size:10px;color:#9cb3c3;text-transform:uppercase;letter-spacing:.14em}.value{margin-top:8px;font-size:18px}.note{margin-top:16px;font-size:12px;color:#9cb3c3}@media(max-width:420px),(max-height:760px){main{padding:16px;border-radius:20px}.speed{font-size:72px}.row{gap:8px;margin-top:12px}.chip{padding:8px 10px;font-size:11px}.card{padding:10px 12px}.value{font-size:16px}.note{font-size:11px}}</style></head><body><main><h1>Drive Orbit</h1><div id="speed" class="speed">0</div><div class="unit">km/h</div><div class="row"><div class="chip" id="link">Link</div><div class="chip" id="fresh">Fresh</div></div><div class="row"><div class="card"><div class="label">RPM</div><div class="value" id="rpm">0</div></div><div class="card"><div class="label">Fuel</div><div class="value" id="fuel">0%</div></div><div class="card"><div class="label">Range</div><div class="value" id="range">0 km</div></div></div><div class="note">Embedded fallback page. Run uploadfs for the full UI bundle.</div></main><script>const speed=document.getElementById('speed');const rpm=document.getElementById('rpm');const fuel=document.getElementById('fuel');const range=document.getElementById('range');const link=document.getElementById('link');const fresh=document.getElementById('fresh');const ws=new WebSocket('ws://'+location.host+'/ws');ws.addEventListener('open',()=>link.textContent='Link');ws.addEventListener('close',()=>link.textContent='Retry');ws.addEventListener('message',event=>{const data=JSON.parse(event.data);speed.textContent=Math.round(data.spd||0);rpm.textContent=Math.round(data.rpm||0);fuel.textContent=Math.round(data.fuel||0)+'%';range.textContent=Math.round(data.rng||0)+' km';fresh.textContent=data.fresh?'Fresh':'Stale';});</script></body></html>)HTML";

constexpr char kEmbeddedIconSvg[] PROGMEM =
  R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><rect width="256" height="256" rx="52" fill="#08111f"/><path d="M48 164a80 80 0 0 1 160 0" fill="none" stroke="#71cfff" stroke-width="20" stroke-linecap="round"/><path d="M80 164a48 48 0 0 1 96 0" fill="none" stroke="#86ffe1" stroke-width="10" stroke-linecap="round"/><path d="M128 128l54 36" fill="none" stroke="#ffb14b" stroke-width="10" stroke-linecap="round"/><circle cx="128" cy="128" r="12" fill="#ebf8ff"/></svg>)SVG";

bool clientAcceptsGzip(AsyncWebServerRequest *request) {
  if (request == nullptr || !request->hasHeader("Accept-Encoding")) {
    return false;
  }

  const AsyncWebHeader *header = request->getHeader("Accept-Encoding");
  if (header == nullptr) {
    return false;
  }

  return header->value().indexOf("gzip") >= 0;
}

size_t serializeSnapshot(const telemetry::CarTelemetry &snapshot, char *buffer, size_t buffer_size) {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  const int written = snprintf(buffer, buffer_size,
                               "{\"v\":1,\"seq\":%lu,\"up\":%lu,\"spd\":%d,\"rpm\":%d,"
                               "\"clt\":%d,\"wt\":%d,\"wc\":%u,\"bat\":%u,\"fuel\":%u,\"rng\":%u,\"acc\":%d,"
                               "\"wifi\":%u,\"hl\":%u,\"obd\":%u,\"fresh\":%u,"
                               "\"app_ws\":%u,\"orb\":%u}",
                               static_cast<unsigned long>(snapshot.sequence),
                               static_cast<unsigned long>(snapshot.uptime_ms), snapshot.speed_kph,
                               snapshot.rpm, snapshot.coolant_temp_c, snapshot.weather_temp_c,
                               snapshot.weather_code, snapshot.battery_mv,
                               snapshot.fuel_level_pct, snapshot.estimated_range_km,
                               snapshot.longitudinal_accel_mg, snapshot.wifi_connected ? 1U : 0U,
                               snapshot.headlights_on ? 1U : 0U, snapshot.obd_connected ? 1U : 0U,
                               snapshot.telemetry_fresh ? 1U : 0U,
                               static_cast<unsigned>(snapshot.app_ws_clients),
                               snapshot.orb_transmitting ? 1U : 0U);
  if (written <= 0 || static_cast<size_t>(written) >= buffer_size) {
    return 0;
  }

  return static_cast<size_t>(written);
}

}  // namespace

DashboardWebServer::DashboardWebServer(const CarTelemetryStore &telemetry_store)
    : telemetry_store_(telemetry_store), server_(80), socket_(kWebSocketPath) {}

void DashboardWebServer::begin(Print &log_output) {
  log_output_ = &log_output;

  if (!LittleFS.begin()) {
    log_output.println("[WEB] LittleFS mount failed, formatting filesystem");
    littlefs_ready_ = LittleFS.begin(true);
  } else {
    littlefs_ready_ = true;
  }

  if (!littlefs_ready_) {
    log_output.println("[WEB] LittleFS unavailable. Static dashboard files will not be served.");
  }

  hosted_ui_available_ = littlefs_ready_ &&
                         (LittleFS.exists("/index.html") || LittleFS.exists("/index.html.gz"));
  if (!hosted_ui_available_) {
    log_output.println("[WEB] Hosted UI missing. Serving embedded fallback until uploadfs runs.");
  }

  socket_.onEvent([this](AsyncWebSocket *server,
                         AsyncWebSocketClient *client,
                         AwsEventType type,
                         void *arg,
                         uint8_t *data,
                         size_t len) { handleSocketEvent(server, client, type, arg, data, len); });
  server_.addHandler(&socket_);

  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/index.html", "text/html; charset=utf-8", false);
  });
  server_.on("/app.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/app.js", "application/javascript; charset=utf-8", false);
  });
  server_.on("/styles.css", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/styles.css", "text/css; charset=utf-8", false);
  });
  server_.on("/icon.svg", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/icon.svg", "image/svg+xml", true);
  });
  server_.on("/icon-maskable.svg", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/icon-maskable.svg", "image/svg+xml", true);
  });
  server_.on("/apple-touch-icon.png", HTTP_GET, [this](AsyncWebServerRequest *request) {
    sendAsset(request, "/apple-touch-icon.png", "image/png", true);
  });
  server_.on(kApiTelemetryPath, HTTP_GET, [this](AsyncWebServerRequest *request) {
    char payload[224] = {0};
    const telemetry::CarTelemetry snapshot = telemetry_store_.snapshot();
    const size_t payload_size = serializeSnapshot(snapshot, payload, sizeof(payload));
    if (payload_size == 0) {
      request->send(500, "application/json", "{\"error\":\"serialize_failed\"}");
      return;
    }
    request->send(200, "application/json", payload);
  });
  server_.onNotFound([this](AsyncWebServerRequest *request) {
    sendAsset(request, "/index.html", "text/html; charset=utf-8", false);
  });

  server_.begin();
  server_started_ = true;
  log_output.println("[WEB] AsyncWebServer ready on port 80");
}

bool DashboardWebServer::hasHostedUi() const {
  return hosted_ui_available_;
}

bool DashboardWebServer::usingEmbeddedFallback() const {
  return !hosted_ui_available_;
}

uint8_t DashboardWebServer::connectedClientCount() const {
  // AsyncWebSocket::count() is const-correct.
  const uint32_t n = socket_.count();
  return n > 255U ? 255U : static_cast<uint8_t>(n);
}

void DashboardWebServer::poll(uint32_t now_ms, bool wifi_connected) {
  if (!server_started_) {
    return;
  }

  if (wifi_connected && !mdns_started_) {
    mdns_started_ = startMdns();
  }

  if (!wifi_connected && last_wifi_connected_ && mdns_started_) {
    MDNS.end();
    mdns_started_ = false;
    if (log_output_ != nullptr) {
      log_output_->println("[WEB] Wi-Fi lost, mDNS stopped");
    }
  }

  last_wifi_connected_ = wifi_connected;
  socket_.cleanupClients();

  if (!wifi_connected) {
    return;
  }

  broadcastSnapshot(now_ms);
}

void DashboardWebServer::handleSocketEvent(AsyncWebSocket *,
                                           AsyncWebSocketClient *client,
                                           AwsEventType type,
                                           void *,
                                           uint8_t *,
                                           size_t) {
  if (client == nullptr) {
    return;
  }

  switch (type) {
    case WS_EVT_CONNECT:
      if (log_output_ != nullptr) {
        log_output_->print("[WEB] WebSocket client connected: #");
        log_output_->println(client->id());
      }
      sendSnapshot(client, telemetry_store_.snapshot());
      break;
    case WS_EVT_DISCONNECT:
      if (log_output_ != nullptr) {
        log_output_->print("[WEB] WebSocket client disconnected: #");
        log_output_->println(client->id());
      }
      break;
    case WS_EVT_DATA:
    case WS_EVT_PING:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void DashboardWebServer::sendSnapshot(AsyncWebSocketClient *client,
                                      const telemetry::CarTelemetry &snapshot) {
  if (client == nullptr || !client->canSend()) {
    return;
  }

  char payload[224] = {0};
  const size_t payload_size = serializeSnapshot(snapshot, payload, sizeof(payload));
  if (payload_size == 0) {
    return;
  }

  client->text(payload, payload_size);
}

void DashboardWebServer::broadcastSnapshot(uint32_t now_ms) {
  if (now_ms - last_broadcast_ms_ < kBroadcastIntervalMs) {
    return;
  }

  last_broadcast_ms_ = now_ms;
  char payload[224] = {0};
  const telemetry::CarTelemetry snapshot = telemetry_store_.snapshot();
  const size_t payload_size = serializeSnapshot(snapshot, payload, sizeof(payload));
  if (payload_size == 0) {
    return;
  }

  socket_.textAll(payload, payload_size);
}

void DashboardWebServer::sendAsset(AsyncWebServerRequest *request,
                                   const char *path,
                                   const char *content_type,
                                   bool cache_forever) {
  if (strcmp(path, "/index.html") == 0 && !hosted_ui_available_) {
    sendEmbeddedAsset(request, "text/html; charset=utf-8", kEmbeddedIndexHtml);
    return;
  }

  if ((strcmp(path, "/icon.svg") == 0 || strcmp(path, "/icon-maskable.svg") == 0) &&
      (!littlefs_ready_ || (!LittleFS.exists(path) && !LittleFS.exists(String(path) + ".gz")))) {
    sendEmbeddedAsset(request, "image/svg+xml", kEmbeddedIconSvg);
    return;
  }

  if (!littlefs_ready_) {
    request->send(500, "text/plain; charset=utf-8", "LittleFS unavailable");
    return;
  }

  String asset_path(path);
  String gzip_path = asset_path + ".gz";
  const bool send_gzip = clientAcceptsGzip(request) && LittleFS.exists(gzip_path);
  const String resolved_path = send_gzip ? gzip_path : asset_path;

  if (!LittleFS.exists(resolved_path)) {
    request->send(404, "text/plain; charset=utf-8", "Not found");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, resolved_path, content_type);
  response->addHeader("Cache-Control", cache_forever ? "public, max-age=31536000, immutable"
                                                     : "public, max-age=60");
  response->addHeader("Content-Disposition", "inline");
  response->addHeader("Vary", "Accept-Encoding");
  if (send_gzip) {
    response->addHeader("Content-Encoding", "gzip");
  }
  request->send(response);
}

void DashboardWebServer::sendEmbeddedAsset(AsyncWebServerRequest *request,
                                           const char *content_type,
                                           const char *content) {
  AsyncWebServerResponse *response = request->beginResponse(200, content_type, content);
  response->addHeader("Cache-Control", "public, max-age=60");
  response->addHeader("Content-Disposition", "inline");
  request->send(response);
}

bool DashboardWebServer::startMdns() {
  if (!MDNS.begin(kHostname)) {
    if (log_output_ != nullptr) {
      log_output_->println("[WEB] mDNS start failed");
    }
    return false;
  }

  MDNS.addService("http", "tcp", 80);
  if (log_output_ != nullptr) {
    log_output_->print("[WEB] mDNS ready at http://");
    log_output_->print(kHostname);
    log_output_->println(".local/");
  }

  return true;
}

}  // namespace app