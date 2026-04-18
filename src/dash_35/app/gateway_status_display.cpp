#include "gateway_status_display.h"

#include <Arduino_GFX_Library.h>
#include <TCA9554.h>
#include <Wire.h>

namespace app {

namespace {

constexpr uint8_t kBacklightPin = 25;
constexpr uint8_t kIoExpanderAddress = 0x20;
constexpr uint8_t kDisplayResetExpanderPin = 0;
constexpr uint8_t kDisplayDcPin = 27;
constexpr uint8_t kDisplayCsPin = 5;
constexpr uint8_t kDisplaySckPin = 18;
constexpr uint8_t kDisplayMosiPin = 23;
constexpr uint8_t kDisplayMisoPin = 19;
constexpr uint8_t kDisplayRotation = 1;
constexpr uint32_t kDisplaySpiHz = 80000000;
constexpr uint16_t kScreenWidth = 480;
constexpr uint16_t kScreenHeight = 320;
constexpr uint32_t kRenderIntervalMs = 400;

TCA9554 io_expander(kIoExpanderAddress);
Arduino_DataBus *display_bus =
    new Arduino_ESP32SPI(kDisplayDcPin, kDisplayCsPin, kDisplaySckPin, kDisplayMosiPin,
                         kDisplayMisoPin);
Arduino_GFX *panel_gfx = new Arduino_ST7796(display_bus, GFX_NOT_DEFINED, 0, true /* ips */);
Arduino_Canvas *framebuffer_gfx = nullptr;
Arduino_GFX *gfx = panel_gfx;

void flushDisplay() {
  if (framebuffer_gfx != nullptr && gfx == framebuffer_gfx) {
    framebuffer_gfx->flush();
  }
}

void drawSectionBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  gfx->drawRoundRect(x, y, w, h, 10, gfx->color565(48, 93, 118));
}

void writeLabel(int16_t x, int16_t y, const char *text) {
  gfx->setTextColor(gfx->color565(135, 173, 190));
  gfx->setTextSize(1);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void writeValue(int16_t x, int16_t y, const char *text, uint8_t size) {
  gfx->setTextColor(gfx->color565(235, 248, 255));
  gfx->setTextSize(size);
  gfx->setCursor(x, y);
  gfx->print(text);
}

}  // namespace

void GatewayStatusDisplay::begin(Stream &log_output) {
  if (initialized_) {
    return;
  }

  Wire.begin();
  io_expander.begin();
  io_expander.pinMode1(kDisplayResetExpanderPin, OUTPUT);
  io_expander.write1(kDisplayResetExpanderPin, LOW);
  delay(20);
  io_expander.write1(kDisplayResetExpanderPin, HIGH);
  delay(200);

  if (!panel_gfx->begin(kDisplaySpiHz)) {
    log_output.println("[DISPLAY] panel begin failed");
    return;
  }

  panel_gfx->setRotation(kDisplayRotation);
  if (framebuffer_gfx == nullptr) {
    framebuffer_gfx = new Arduino_Canvas(kScreenWidth, kScreenHeight, panel_gfx);
    if (framebuffer_gfx != nullptr && framebuffer_gfx->begin(GFX_SKIP_OUTPUT_BEGIN)) {
      gfx = framebuffer_gfx;
      log_output.println("[DISPLAY] canvas back buffer enabled");
    } else {
      delete framebuffer_gfx;
      framebuffer_gfx = nullptr;
      gfx = panel_gfx;
      log_output.println("[DISPLAY] canvas unavailable, using direct panel draws");
    }
  }

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, HIGH);
  gfx->fillScreen(gfx->color565(4, 9, 19));
  flushDisplay();

  initialized_ = true;
  log_output.println("[DISPLAY] gateway status display ready");
}

void GatewayStatusDisplay::render(const telemetry::CarTelemetry &telemetry,
                                  float trip_distance_km,
                                  const char *access_label,
                                  bool hosted_ui_ready,
                                  bool using_embedded_ui,
                                  bool simulation_enabled,
                                  uint32_t now_ms) {
  if (!initialized_ || now_ms - last_render_ms_ < kRenderIntervalMs) {
    return;
  }

  last_render_ms_ = now_ms;
  gfx->fillScreen(gfx->color565(4, 9, 19));

  gfx->setTextColor(gfx->color565(134, 255, 225));
  gfx->setTextSize(1);
  gfx->setCursor(18, 18);
  gfx->print("DRIVE ORBIT");

  gfx->setTextColor(gfx->color565(235, 248, 255));
  gfx->setTextSize(2);
  gfx->setCursor(18, 38);
  gfx->print("Gateway View");

  const uint16_t badge_color = simulation_enabled
                                   ? gfx->color565(255, 177, 75)
                                   : hosted_ui_ready ? gfx->color565(109, 246, 216)
                                                     : gfx->color565(255, 177, 75);
  gfx->drawRoundRect(334, 16, 128, 28, 10, badge_color);
  gfx->setTextColor(badge_color);
  gfx->setTextSize(1);
  gfx->setCursor(346, 26);
  gfx->print(simulation_enabled ? "SIM MODE" : hosted_ui_ready ? "WEB READY" : "UPLOAD UI");

  drawSectionBox(18, 70, 444, 124);
  writeLabel(34, 90, "SPEED");
  char speed_text[12] = {0};
  snprintf(speed_text, sizeof(speed_text), "%d", telemetry.speed_kph);
  writeValue(34, 118, speed_text, 5);
  writeLabel(210, 152, "KM/H");

  writeLabel(322, 90, "RPM");
  char rpm_text[16] = {0};
  snprintf(rpm_text, sizeof(rpm_text), "%d", telemetry.rpm);
  writeValue(322, 116, rpm_text, 2);

  writeLabel(322, 154, "TRIP");
  char trip_text[24] = {0};
  snprintf(trip_text, sizeof(trip_text), "%.1f km", trip_distance_km);
  writeValue(322, 174, trip_text, 2);

  drawSectionBox(18, 208, 140, 94);
  writeLabel(30, 226, "FUEL");
  char fuel_text[12] = {0};
  snprintf(fuel_text, sizeof(fuel_text), "%u%%", telemetry.fuel_level_pct);
  writeValue(30, 248, fuel_text, 2);

  drawSectionBox(170, 208, 140, 94);
  writeLabel(182, 226, "RANGE");
  char range_text[18] = {0};
  snprintf(range_text, sizeof(range_text), "%u km", telemetry.estimated_range_km);
  writeValue(182, 248, range_text, 2);

  drawSectionBox(322, 208, 140, 94);
  writeLabel(334, 226, "ACCESS");
  gfx->setTextColor(gfx->color565(235, 248, 255));
  gfx->setTextSize(1);
  gfx->setCursor(334, 246);
  gfx->print(access_label != nullptr ? access_label : "waiting...");
  gfx->setCursor(334, 266);
  gfx->print(using_embedded_ui ? "embedded page" : "filesystem page");

  // Phone app WebSocket connections
  {
    const bool has_app = telemetry.app_ws_clients > 0;
    gfx->setTextColor(has_app ? gfx->color565(109, 246, 216) : gfx->color565(135, 173, 190));
    gfx->setTextSize(1);
    gfx->setCursor(334, 286);
    char app_text[18] = {0};
    snprintf(app_text, sizeof(app_text), "APP: %u conn", telemetry.app_ws_clients);
    gfx->print(app_text);
  }

  gfx->setTextColor(gfx->color565(135, 173, 190));
  gfx->setTextSize(1);
  gfx->setCursor(20, 306);
  gfx->print(telemetry.wifi_connected ? "Wi-Fi linked" : "Wi-Fi waiting");
  gfx->setCursor(156, 306);
  gfx->print(telemetry.obd_connected ? "OBD live" : "OBD scan");
  gfx->setCursor(268, 306);
  gfx->print(telemetry.telemetry_fresh ? "data fresh" : "data stale");

  // Orb ESP-NOW link indicator
  gfx->setTextColor(telemetry.orb_transmitting
                        ? gfx->color565(109, 246, 216)
                        : gfx->color565(135, 173, 190));
  gfx->setCursor(380, 306);
  gfx->print(telemetry.orb_transmitting ? "ORB link" : "ORB off");
  flushDisplay();
}

}  // namespace app