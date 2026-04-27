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
  R"HTML(<!doctype html><html lang="en" data-theme="dark"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,viewport-fit=cover"><meta name="theme-color" content="#0c1116"><title>Drive Orbit</title><style>:root{--bg:#0b1116;--panel:#0f1722;--panel-strong:#111a27;--text:#eef6ff;--muted:#9ca3af;--dim:#64758b;--rule:rgba(255,255,255,.08);--teal:#7df9c6;--amber:#fbbf24;--rose:#fb7185}body[data-theme="black"]{--bg:#000;--panel:#05070b;--panel-strong:#090d12;--text:#f6fbff;--muted:#9ba6b2;--dim:#6a7480}*{box-sizing:border-box}html,body{margin:0;height:100%;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;overflow:hidden}body{display:grid;place-items:center;padding:12px}main{width:min(92vw,460px);padding:16px;border:1px solid var(--rule);border-radius:24px;background:linear-gradient(180deg,var(--panel),var(--panel-strong));box-shadow:0 24px 60px rgba(0,0,0,.28)}.top{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;margin-bottom:12px}.copy{display:flex;flex-direction:column;gap:4px}.kicker{font-size:10px;letter-spacing:.3em;text-transform:uppercase;color:var(--dim)}h1{margin:0;font-size:18px;letter-spacing:.08em}.actions{display:flex;gap:10px;align-items:center}.btn,.wifi{width:34px;height:34px;border-radius:50%;border:1px solid var(--rule);background:var(--panel);display:grid;place-items:center;color:var(--muted);cursor:pointer}.btn{width:auto;padding:0 12px;border-radius:999px;font-size:10px;letter-spacing:.18em;text-transform:uppercase;color:var(--text)}.wifi svg{width:18px;height:18px;stroke:currentColor;fill:none}.wifi.live{color:var(--teal);border-color:rgba(125,249,198,.35)}.wifi.error{color:var(--rose);border-color:rgba(251,113,133,.35)}.weather{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 14px;border:1px solid var(--rule);border-radius:18px;background:linear-gradient(180deg,var(--panel),var(--panel-strong));margin-bottom:12px}.w-icon{font-size:24px}.w-copy{display:flex;flex-direction:column;gap:2px;flex:1}.w-temp{font-size:20px;font-weight:600}.w-cond,.w-note{font-size:10px;letter-spacing:.16em;text-transform:uppercase;color:var(--muted)}.grid{display:grid;grid-template-columns:1.15fr .85fr .85fr;gap:8px;margin-top:12px}.speed{font-size:min(28vw,92px);line-height:.86;font-weight:700;letter-spacing:-.04em}.unit{font-size:11px;letter-spacing:.4em;text-transform:uppercase;color:var(--dim)}.card{padding:12px 14px;border:1px solid var(--rule);border-radius:16px;background:rgba(255,255,255,.02);min-width:0}.label{font-size:9px;letter-spacing:.14em;text-transform:uppercase;color:var(--dim)}.value{margin-top:8px;font-size:18px}.row{display:flex;justify-content:space-between;gap:10px;margin-top:12px}.chip{padding:10px 12px;border-radius:999px;background:rgba(255,255,255,.05);font-size:12px;color:var(--muted)}.note{margin-top:14px;font-size:11px;color:var(--dim)}@media(max-width:420px),(max-height:760px){main{padding:14px;border-radius:20px}.grid{gap:6px}.value{font-size:16px}.speed{font-size:72px}.weather{padding:10px 12px}}</style></head><body data-theme="dark"><main><div class="top"><div class="copy"><span class="kicker">Connected weather</span><h1>Drive Orbit</h1></div><div class="actions"><button class="btn" id="theme-toggle" type="button">BLACK</button><div class="wifi live" id="wifi-status" title="Wi-Fi linked" aria-label="Wi-Fi linked"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 10a11 11 0 0 1 14 0" stroke-width="2" stroke-linecap="round"/><path d="M8 13a7 7 0 0 1 8 0" stroke-width="2" stroke-linecap="round"/><path d="M11 16a3 3 0 0 1 2 0" stroke-width="2" stroke-linecap="round"/><circle cx="12" cy="19" r="1.5" fill="currentColor" stroke="none"/></svg></div></div></div><section class="weather"><div class="w-icon" id="weather-icon">☁</div><div class="w-copy"><div class="w-temp" id="weather-temp">--°C</div><div class="w-cond" id="weather-cond">weather offline</div><div class="w-note" id="weather-note">Wi-Fi • open-meteo</div></div></section><div class="speed" id="speed">0</div><div class="unit">km/h</div><div class="grid"><div class="card"><div class="label">RPM</div><div class="value" id="rpm">0</div></div><div class="card"><div class="label">Fuel</div><div class="value" id="fuel">0%</div></div><div class="card"><div class="label">Range</div><div class="value" id="range">0 km</div></div></div><div class="row"><div class="chip" id="link">Link</div><div class="chip" id="fresh">Fresh</div></div><div class="note">Embedded fallback page. Run uploadfs for the full UI bundle.</div></main><script>const speed=document.getElementById('speed'),rpm=document.getElementById('rpm'),fuel=document.getElementById('fuel'),range=document.getElementById('range'),link=document.getElementById('link'),fresh=document.getElementById('fresh'),themeToggle=document.getElementById('theme-toggle'),wifi=document.getElementById('wifi-status'),wIcon=document.getElementById('weather-icon'),wTemp=document.getElementById('weather-temp'),wCond=document.getElementById('weather-cond'),wNote=document.getElementById('weather-note');themeToggle.addEventListener('click',()=>{const next=document.body.dataset.theme==='black'?'dark':'black';document.body.dataset.theme=next;themeToggle.textContent=next==='black'?'DARK':'BLACK'});const weatherMeta={0:['☀','clear'],1:['🌤','mainly clear'],2:['⛅','partly cloudy'],3:['☁','cloudy'],45:['🌫','fog'],48:['🌫','fog'],51:['🌦','drizzle'],61:['🌧','rain'],71:['❄','snow'],80:['🌧','showers'],82:['⛈','storm'],95:['⛈','storm']};const ws=new WebSocket('ws://'+location.host+'/ws');ws.addEventListener('open',()=>{link.textContent='Link';wifi.className='wifi live';wifi.title='Wi-Fi linked'});ws.addEventListener('close',()=>{link.textContent='Retry';wifi.className='wifi error';wifi.title='Wi-Fi offline'});ws.addEventListener('message',event=>{const data=JSON.parse(event.data),meta=weatherMeta[data.wc]||['☁','weather'];speed.textContent=Math.round(data.spd||0);rpm.textContent=Math.round(data.rpm||0);fuel.textContent=Math.round(data.fuel||0)+'%';range.textContent=Math.round(data.rng||0)+' km';fresh.textContent=data.fresh?'Fresh':'Stale';wIcon.textContent=meta[0];wTemp.textContent=(data.wt==null?'--':Math.round(data.wt))+'°C';wCond.textContent=meta[1];wNote.textContent=(data.wifi?'Wi-Fi':'Wi-Fi offline')+' • open-meteo';wifi.className='wifi '+(data.wifi?'live':'error');wifi.title=data.wifi?'Wi-Fi linked':'Wi-Fi offline'});</script></body></html>)HTML";

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