#include "orb_display.h"

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>

#include "cyber_theme.h"

namespace orb {

namespace {

constexpr int kPinSclk = 6;
constexpr int kPinMosi = 7;
constexpr int kPinCs   = 10;
constexpr int kPinDc   = 2;
constexpr int kPinRst  = 1;
constexpr int kPinBl   = 3;

constexpr int kW = 240;
constexpr int kH = 240;
constexpr int kCx = kW / 2;
constexpr int kCy = kH / 2;

Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t COLOR_BG = ((12 & 0xF8) << 8) | ((17 & 0xFC) << 3) | (22 >> 3);
const uint16_t COLOR_GAS_ARC = ((238 & 0xF8) << 8) | ((246 & 0xFC) << 3) | (255 >> 3);
const uint16_t COLOR_TRIP_LBL = ((122 & 0xF8) << 8) | ((153 & 0xFC) << 3) | (173 >> 3);
const uint16_t COLOR_TRIP_VAL = ((73 & 0xF8) << 8) | ((199 & 0xFC) << 3) | (255 >> 3);
const uint16_t COLOR_RNG_LBL = ((76 & 0xF8) << 8) | ((175 & 0xFC) << 3) | (140 >> 3);
const uint16_t COLOR_RNG_VAL = ((125 & 0xF8) << 8) | ((249 & 0xFC) << 3) | (198 >> 3);
const uint16_t COLOR_WHITE = 0xFFFF;
const uint16_t COLOR_DIM = ((25 & 0xF8) << 8) | ((25 & 0xFC) << 3) | (25 >> 3);

void drawArc(int cx, int cy, int r_outer, int r_inner,
             float start_deg, float end_deg, uint16_t color, bool dashed = false) {
  if (end_deg < start_deg) return;
  for (float a = start_deg; a <= end_deg; a += 0.8f) {
    if (dashed && (static_cast<int>(a) % 12 < 4)) continue; // 8 draw, 4 skip
    float rad = a * DEG_TO_RAD;
    float cs = cosf(rad);
    float sn = sinf(rad);
    for (int r = r_inner; r <= r_outer; ++r) {
      int x = cx + static_cast<int>(cs * r);
      int y = cy + static_cast<int>(sn * r);
      if (x >= 0 && x < kW && y >= 0 && y < kH) {
        gfx->drawPixel(x, y, color);
      }
    }
  }
}

// Draw text centered, filling rect behind it to avoid flicker
void renderCenteredText(int x, int y, int w, int h, const char* text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x - w/2, y - h/2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  // Estimation: char width = 6 * size, height = 8 * size
  int text_w = strlen(text) * 6 * size;
  int text_h = 8 * size;
  gfx->setCursor(x - text_w/2, y - text_h/2);
  gfx->print(text);
}

// Right aligned text
void renderRightText(int x, int y, int w, int h, const char* text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x - w, y - h/2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  int text_w = strlen(text) * 6 * size;
  int text_h = 8 * size;
  gfx->setCursor(x - text_w, y - text_h/2);
  gfx->print(text);
}

// Left aligned text
void renderLeftText(int x, int y, int w, int h, const char* text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x, y - h/2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  int text_h = 8 * size;
  gfx->setCursor(x, y - text_h/2);
  gfx->print(text);
}

}  // namespace

void OrbDisplay::begin(Print &log) {
  log_ = &log;
  pinMode(kPinBl, OUTPUT);
  analogWrite(kPinBl, 255);

  bus = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, -1);
  gfx = new Arduino_GC9A01(bus, kPinRst, 0, true);
  gfx->begin();
  
  gfx->fillScreen(COLOR_BG);
  initialized_ = true;
  last_render_ms_ = 0;
  log_->println("[DISP] Data Gauge Mode Ready");
}

void OrbDisplay::updateBacklight(uint8_t drive_mode, uint32_t now_ms) {
  if (now_ms - last_backlight_ms_ < 200) return;
  last_backlight_ms_ = now_ms;
  uint8_t target = 255;
  if (in_no_signal_) target = 80;
  if (backlight_level_ < target)
    backlight_level_ = min(static_cast<uint8_t>(backlight_level_ + 8), target);
  else if (backlight_level_ > target)
    backlight_level_ = max(static_cast<uint8_t>(backlight_level_ - 4), target);
  analogWrite(kPinBl, backlight_level_);
}

void OrbDisplay::renderNoSignal(uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_no_signal_render_ms_ < 1000) return;
  last_no_signal_render_ms_ = now_ms;

  if (!in_no_signal_ || in_debug_screen_) {
    in_debug_screen_ = false;
    gfx->fillScreen(COLOR_BG);
    in_no_signal_ = true;
    
    // Draw static elements once
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TRIP_LBL, COLOR_BG);
    
    renderCenteredText(kCx, kCy + 32, 60, 16, "KM/H", COLOR_TRIP_LBL, 1);
    renderRightText(kCx - 10, kCy + 58, 40, 10, "TRIP", COLOR_TRIP_LBL, 1);
    renderLeftText(kCx + 10, kCy + 58, 40, 10, "RNG", COLOR_RNG_LBL, 1);
    gfx->fillRect(kCx - 1, kCy + 54, 2, 20, ((50 & 0xF8) << 8) | ((50 & 0xFC) << 3) | (50 >> 3));

    // Draw "--" for the dynamic values
    renderCenteredText(kCx, kCy, 120, 60, "--", COLOR_WHITE, 6);
    renderRightText(kCx - 10, kCy + 70, 48, 12, "--.-", COLOR_WHITE, 1);
    renderLeftText(kCx + 10, kCy + 70, 32, 12, "--", COLOR_WHITE, 1);
    
    // Draw empty arcs
    drawArc(kCx, kCy, 118, 114, 90.0f, 180.0f, COLOR_BG, false);
    drawArc(kCx, kCy, 118, 114, -90.0f, 0.0f, COLOR_BG, false);
    
    // Invalidate cached state so render() redraws when signal recovers
    last_spd_ = -1;
    last_gas_ = -1.0f;
    last_lod_ = -1.0f;
  }

  // Optionally blink a connecting indicator? For now just keep it clean
  updateBacklight(0, now_ms);
}

void OrbDisplay::render(const PetState &pet, const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_render_ms_ < 100) return; // Cap at 10 FPS
  last_render_ms_ = now_ms;

  if (in_no_signal_ || in_debug_screen_) {
    in_debug_screen_ = false;
    in_no_signal_ = false; // ensure reset
    gfx->fillScreen(COLOR_BG);
    in_no_signal_ = false;
    
    // Draw static elements once on recovery
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TRIP_LBL, COLOR_BG);
    
    // KM/H underneath speed
    renderCenteredText(kCx, kCy + 32, 60, 16, "KM/H", COLOR_TRIP_LBL, 1);
    
    // Bottom Trip/Rng labels
    renderRightText(kCx - 10, kCy + 58, 40, 10, "TRIP", COLOR_TRIP_LBL, 1);
    renderLeftText(kCx + 10, kCy + 58, 40, 10, "RNG", COLOR_RNG_LBL, 1);
    gfx->fillRect(kCx - 1, kCy + 54, 2, 20, rgb565(50, 50, 50));
  }

  updateBacklight(static_cast<uint8_t>(t.drive_mode), now_ms);

  float raw_gas = t.fuel_level_pct / 100.0f;
  // Compute artificial variables if missing from basic OBD2
  float load_raw = (float)t.rpm / 7000.0f;
  if (load_raw > 1.0f) load_raw = 1.0f;
  if (load_raw < 0.0f) load_raw = 0.0f;
  
  // Exponential smoothing
  smooth_gas_ = smooth_gas_ + (raw_gas - smooth_gas_) * 0.1f;
  smooth_lod_ = smooth_lod_ + (load_raw - smooth_lod_) * 0.3f;

  // Track an artificial trip locally just for presentation since standard OBD2 doesn't have it 
  static float simulated_trip = 12.4f;
  static uint32_t last_trip_ms = now_ms;
  float dt_h = (now_ms - last_trip_ms) / 3600000.0f;
  simulated_trip += t.speed_kph * dt_h;
  last_trip_ms = now_ms;

  int range_km = (int)(420.0f * smooth_gas_);

  char buf[32];

  // 1. SPEED
  if (t.speed_kph != last_spd_ || smooth_gas_ != last_gas_) {
    last_spd_ = t.speed_kph;
    snprintf(buf, sizeof(buf), "%d", t.speed_kph);
    // Draw huge speed text
    renderCenteredText(kCx, kCy, 120, 60, buf, COLOR_WHITE, 6);
  }

  // 2. BOTTOM TRIP / RNG
  if (true) {
    snprintf(buf, sizeof(buf), "%.1f", simulated_trip);
    renderRightText(kCx - 10, kCy + 70, 48, 12, buf, COLOR_WHITE, 1);
    
    snprintf(buf, sizeof(buf), "%d", range_km);
    renderLeftText(kCx + 10, kCy + 70, 32, 12, buf, COLOR_WHITE, 1);

    // Range Bar
    gfx->fillRect(kCx - 30, kCy + 82, 60, 2, COLOR_DIM);
    gfx->fillRect(kCx - 30, kCy + 82, 60 * smooth_gas_, 2, COLOR_RNG_VAL);
  }

  // 3. GAS ARC (Bottom left 90 deg -> 180 deg)
  // 90 deg is bottom, sweeping left to 180 deg. Wait, `Math.PI/2` is 90 deg.
  // In the HTML JS: `arc(CX, CY, 118, Math.PI/2, Math.PI/2 + Math.PI/2*gas)` sweeping clockwise.
  // 90 is bottom, to 180 (left). 
  if (fabs(smooth_gas_ - last_gas_) > 0.01f) {
    float gas_sweep = smooth_gas_ * 90.0f;
    // Overwrite the whole 90 deg quadrant first to "clear" it
    drawArc(kCx, kCy, 118, 114, 90.0f, 180.0f, COLOR_BG, false);
    drawArc(kCx, kCy, 118, 114, 90.0f, 90.0f + gas_sweep, COLOR_GAS_ARC, true);
    
    float tip_rad = (90.0f + gas_sweep) * DEG_TO_RAD;
    int gx = kCx + static_cast<int>(cosf(tip_rad) * 100.0f);
    int gy = kCy + static_cast<int>(sinf(tip_rad) * 100.0f);
    
    snprintf(buf, sizeof(buf), "%d%%", (int)(smooth_gas_ * 100.0f));
    renderCenteredText(gx, gy, 30, 16, buf, COLOR_WHITE, 1);
    
    last_gas_ = smooth_gas_;
  }

  // 4. LOAD ARC (Top Right -90 deg -> 0 deg)
  // In JS: `arc(CX, CY, 118, -Math.PI/2, -Math.PI/2 + Math.PI/2*load)`
  if (fabs(smooth_lod_ - last_lod_) > 0.01f) {
    float lod_sweep = smooth_lod_ * 90.0f;
    drawArc(kCx, kCy, 118, 114, -90.0f, 0.0f, COLOR_BG, false);
    
    uint8_t r = 255;
    uint8_t g_col = 200 - (uint8_t)(smooth_lod_ * 150.0f);
    uint16_t lod_color = rgb565(r, g_col, 0);
    
    drawArc(kCx, kCy, 118, 114, -90.0f, -90.0f + lod_sweep, lod_color, false);
    last_lod_ = smooth_lod_;
  }
}



void OrbDisplay::renderDebugScreen(const char* status, int16_t speed, int16_t rpm, uint8_t fuel, uint32_t packets, uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_render_ms_ < 200) return; // 5 FPS for debug
  last_render_ms_ = now_ms;

  if (!in_debug_screen_) {
    gfx->fillScreen(COLOR_BG);
    in_debug_screen_ = true;
    in_no_signal_ = false;
    
    // Draw static title
    gfx->setTextColor(COLOR_RNG_VAL, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(kCx - 60, 40);
    gfx->print("DEBUG INFO");
  }

  // Draw text with background color replacing pixels to avoid flicker
  gfx->setTextColor(COLOR_WHITE, COLOR_BG);
  gfx->setTextSize(1);
  
  int y = 80;
  int d_y = 20;

  char buf[64];
  
  gfx->setCursor(40, y);
  snprintf(buf, sizeof(buf), "Status: %-15s", status);
  gfx->print(buf);
  y += d_y;

  gfx->setCursor(40, y);
  snprintf(buf, sizeof(buf), "Speed:  %-5d km/h", speed);
  gfx->print(buf);
  y += d_y;

  gfx->setCursor(40, y);
  snprintf(buf, sizeof(buf), "RPM:    %-5d     ", rpm);
  gfx->print(buf);
  y += d_y;

  gfx->setCursor(40, y);
  snprintf(buf, sizeof(buf), "Fuel:   %-3d %%     ", fuel);
  gfx->print(buf);
  y += d_y;

  gfx->setCursor(40, y);
  snprintf(buf, sizeof(buf), "Pkts:   %-10u", packets);
  gfx->print(buf);
  
  updateBacklight(0, now_ms);
}
}  // namespace orb

