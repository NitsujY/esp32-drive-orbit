#include "orb_display.h"

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>

#include "cyber_theme.h"

namespace orb {

// ── Hardware ─────────────────────────────────────────────────────────────────

namespace {

constexpr int kPinSclk = 6;
constexpr int kPinMosi = 7;
constexpr int kPinCs   = 10;
constexpr int kPinDc   = 2;
constexpr int kPinRst  = 1;
constexpr int kPinBl   = 3;

constexpr int kW  = 240;
constexpr int kH  = 240;
constexpr int kCx = kW / 2;
constexpr int kCy = kH / 2;

Arduino_DataBus *bus = nullptr;
Arduino_GFX     *gfx = nullptr;

// ── Colour helpers ────────────────────────────────────────────────────────────

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Fixed palette — these never change with theme on the orb (it's always dark).
const uint16_t COLOR_BG       = rgb565(12,  17,  22);
const uint16_t COLOR_GAS_ARC  = rgb565(238, 246, 255);
const uint16_t COLOR_TRIP_LBL = rgb565(122, 153, 173);
const uint16_t COLOR_TRIP_VAL = rgb565(73,  199, 255);
const uint16_t COLOR_RNG_LBL  = rgb565(76,  175, 140);
const uint16_t COLOR_RNG_VAL  = rgb565(125, 249, 198);
const uint16_t COLOR_WHITE    = 0xFFFF;
const uint16_t COLOR_DIM      = rgb565(25,  25,  25);
// Parked-screen accent colour
const uint16_t COLOR_TEAL     = rgb565(45,  212, 191);
const uint16_t COLOR_AMBER    = rgb565(255, 191, 0);

// ── Drawing primitives ────────────────────────────────────────────────────────

void drawArc(int cx, int cy, int r_outer, int r_inner,
             float start_deg, float end_deg, uint16_t color,
             bool dashed = false) {
  if (end_deg < start_deg) return;
  for (float a = start_deg; a <= end_deg; a += 0.8f) {
    if (dashed && (static_cast<int>(a) % 12 < 4)) continue;
    const float rad = a * DEG_TO_RAD;
    const float cs  = cosf(rad);
    const float sn  = sinf(rad);
    for (int r = r_inner; r <= r_outer; ++r) {
      const int x = cx + static_cast<int>(cs * r);
      const int y = cy + static_cast<int>(sn * r);
      if (x >= 0 && x < kW && y >= 0 && y < kH)
        gfx->drawPixel(x, y, color);
    }
  }
}

// Centred text — clears a rect behind it to prevent flicker.
void renderCenteredText(int x, int y, int w, int h,
                        const char *text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x - w / 2, y - h / 2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  const int tw = strlen(text) * 6 * size;
  const int th = 8 * size;
  gfx->setCursor(x - tw / 2, y - th / 2);
  gfx->print(text);
}

void renderRightText(int x, int y, int w, int h,
                     const char *text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x - w, y - h / 2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  const int tw = strlen(text) * 6 * size;
  const int th = 8 * size;
  gfx->setCursor(x - tw, y - th / 2);
  gfx->print(text);
}

void renderLeftText(int x, int y, int w, int h,
                    const char *text, uint16_t fg, uint8_t size = 1) {
  gfx->fillRect(x, y - h / 2, w, h, COLOR_BG);
  gfx->setTextColor(fg, COLOR_BG);
  gfx->setTextSize(size);
  const int th = 8 * size;
  gfx->setCursor(x, y - th / 2);
  gfx->print(text);
}

// Derive HH:MM from millis() uptime.
// The orb has no RTC; this gives a relative clock that resets on boot.
// When the dash_35 pushes real wall-clock time via a future telemetry field
// this can be replaced — for now uptime is the best available source.
void getWallClockHHMM(uint8_t &hh_out, uint8_t &mm_out) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    hh_out = 0;
    mm_out = 0;
    return;
  }
  hh_out = timeinfo.tm_hour;
  mm_out = timeinfo.tm_min;
}

// Draw the static speed-screen chrome (labels + divider).
// Called once on entry to the speed screen.
void drawSpeedScreenChrome() {
  renderCenteredText(kCx, kCy + 32, 60, 16, "KM/H", COLOR_TRIP_LBL, 1);
  renderRightText(kCx - 10, kCy + 58, 40, 10, "TRIP", COLOR_TRIP_LBL, 1);
  renderLeftText(kCx + 10,  kCy + 58, 40, 10, "RNG",  COLOR_RNG_LBL,  1);
  gfx->fillRect(kCx - 1, kCy + 54, 2, 20, rgb565(50, 50, 50));
}

}  // namespace

// ── begin ─────────────────────────────────────────────────────────────────────

void OrbDisplay::begin(Print &log) {
  log_ = &log;
  pinMode(kPinBl, OUTPUT);
  analogWrite(kPinBl, 255);

  bus = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, -1);
  gfx = new Arduino_GC9A01(bus, kPinRst, 0, true);
  gfx->begin();
  gfx->fillScreen(COLOR_BG);

  initialized_     = true;
  last_render_ms_  = 0;
  
  // Splash Screen so the screen isn't black
  gfx->setTextColor(COLOR_WHITE, COLOR_BG);
  gfx->setTextSize(2);
  const char* title = "ORBIT OS";
  int tw = strlen(title) * 6 * 2;
  gfx->setCursor(kCx - tw / 2, kCy - 8);
  gfx->print(title);
  
  // Start in no-signal mode, which will flip over to normal graphics
  in_no_signal_    = true;

  log_->println("[DISP] Data Gauge Mode Ready");
}

// ── updateBacklight ───────────────────────────────────────────────────────────

void OrbDisplay::updateBacklight(uint8_t /*drive_mode*/, uint32_t now_ms) {
  if (now_ms - last_backlight_ms_ < 200) return;
  last_backlight_ms_ = now_ms;

  const uint8_t target = in_no_signal_ ? 80 : 255;
  if (backlight_level_ < target)
    backlight_level_ = static_cast<uint8_t>(min((int)backlight_level_ + 8, (int)target));
  else if (backlight_level_ > target)
    backlight_level_ = static_cast<uint8_t>(max((int)backlight_level_ - 4, (int)target));

  analogWrite(kPinBl, backlight_level_);
}

// ── renderNoSignal ────────────────────────────────────────────────────────────

void OrbDisplay::renderNoSignal(uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_no_signal_render_ms_ < 1000) return;
  last_no_signal_render_ms_ = now_ms;

  if (!in_no_signal_ || in_debug_screen_) {
    in_debug_screen_  = false;
    in_parked_screen_ = false;
    gfx->fillScreen(COLOR_BG);
    in_no_signal_ = true;

    drawSpeedScreenChrome();
    renderCenteredText(kCx, kCy,        120, 60, "--",   COLOR_WHITE, 6);
    renderRightText(kCx - 10, kCy + 70,  48, 12, "--.-", COLOR_WHITE, 1);
    renderLeftText(kCx + 10,  kCy + 70,  32, 12, "--",   COLOR_WHITE, 1);

    drawArc(kCx, kCy, 118, 114,  90.0f, 180.0f, COLOR_BG, false);
    drawArc(kCx, kCy, 118, 114, -90.0f,   0.0f, COLOR_BG, false);

    last_spd_ = -1;
    last_gas_ = -1.0f;
    last_lod_ = -1.0f;
  }

  updateBacklight(0, now_ms);
}

// ── renderParkedScreen ────────────────────────────────────────────────────────
//
// Minimal layout — clock + temperature only. Gas arc stays on canvas.
//
//   HH:MM    size-3  y = kCy - 14   (centred vertically)
//   temp °C  size-2  y = kCy + 18   (teal, below clock)
//
// Refreshes at 1 Hz; only redraws fields that actually changed.

void OrbDisplay::renderParkedScreen(const telemetry::DashboardTelemetry &t,
                                    uint32_t now_ms) {
  if (now_ms - last_parked_render_ms_ < kParkedRefreshMs) return;
  last_parked_render_ms_ = now_ms;

  char buf[64];

  // Sunset logic: use amber if it's past sunset or before 6 AM
  uint8_t hh, mm;
  getWallClockHHMM(hh, mm);
  uint16_t curr_min = (hh * 60) + mm;
  bool is_night = false;
  if (t.sunset_minute != 0xFFFF) {
    if (curr_min >= t.sunset_minute || hh < 6) is_night = true;
  } else {
    if (hh >= 18 || hh < 6) is_night = true; // Fallback 18:00 to 06:00
  }
  uint16_t main_color = is_night ? COLOR_AMBER : COLOR_WHITE;

  // ── Greeting ──
  if (t.greeting[0] != '\0' && !greeting_shown_) {
    if (greeting_start_ms_ == 0) {
      greeting_start_ms_ = now_ms;
      gfx->fillScreen(COLOR_BG);
    }
    
    if (now_ms - greeting_start_ms_ < 5000) {
      // Just print it simply in the middle of the screen
      gfx->fillRect(20, kCy - 20, kW - 40, 40, COLOR_BG);
      gfx->setTextColor(main_color, COLOR_BG);
      gfx->setTextSize(1);
      
      // Basic wrapping
      int y = kCy - 10;
      String g(t.greeting);
      while(g.length() > 0) {
        String chunk = g.substring(0, 30);
        gfx->setCursor(kCx - (chunk.length() * 6) / 2, y);
        gfx->print(chunk);
        g = g.length() > 30 ? g.substring(30) : "";
        y += 10;
      }
      return;
    } else {
      greeting_shown_ = true;
      gfx->fillScreen(COLOR_BG);
      // Force redrawing the gas arc and parked screen chrome on next passes
      last_park_hh_   = 0xFF;
      last_park_mm_   = 0xFF;
      last_park_temp_ = static_cast<int8_t>(telemetry::kWeatherTempUnknown - 1);
      last_gas_ = -1.0f; // Force redraw of gas arc
    }
  }

  // ── Clock ──
  if (hh != last_park_hh_ || mm != last_park_mm_ || is_night != last_night_mode_) {
    last_park_hh_ = hh;
    last_park_mm_ = mm;
    last_night_mode_ = is_night;
    snprintf(buf, sizeof(buf), "%02u:%02u", hh, mm);
    // size-3: char 18 px wide, 24 px tall → 5 chars = 90 px wide
    renderCenteredText(kCx, kCy - 14, 96, 26, buf, main_color, 3);
  }

  // ── Temperature ──
  if (t.weather_temp_c != last_park_temp_) {
    last_park_temp_ = t.weather_temp_c;
    if (t.weather_temp_c == telemetry::kWeatherTempUnknown) {
      snprintf(buf, sizeof(buf), "--\xF7" "C");
    } else {
      snprintf(buf, sizeof(buf), "%d\xF7" "C", static_cast<int>(t.weather_temp_c));
    }
    // size-2: char 12 px wide → up to 5 chars = 60 px
    renderCenteredText(kCx, kCy + 18, 72, 18, buf, COLOR_TEAL, 2);
  }
}

// ── render ────────────────────────────────────────────────────────────────────

void OrbDisplay::render(const PetState & /*pet*/,
                        const telemetry::DashboardTelemetry &t,
                        uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_render_ms_ < 100) return;  // cap at 10 FPS
  last_render_ms_ = now_ms;

  // ── Recovery from no-signal / debug ──────────────────────────────────────
  if (in_no_signal_ || in_debug_screen_) {
    in_debug_screen_ = false;
    in_no_signal_    = false;
    in_parked_screen_ = false; // Force proper transition
    gfx->fillScreen(COLOR_BG);
    drawSpeedScreenChrome(); // Base chrome for both screens
    last_spd_ = -1;
    last_gas_ = -1.0f;
    last_lod_ = -1.0f;
  }

  // ── Parked-screen state machine ───────────────────────────────────────────
  if (t.speed_kph == 0) {
    if (stopped_since_ms_ == 0) stopped_since_ms_ = now_ms;
  } else {
    stopped_since_ms_ = 0;
  }

  const bool should_park = (t.speed_kph == 0) &&
                           (stopped_since_ms_ > 0) &&
                           ((now_ms - stopped_since_ms_) >= kParkDelayMs);

  // Transition: driving → parked
  if (should_park && !in_parked_screen_) {
    in_parked_screen_ = true;
    in_no_signal_     = false;
    in_debug_screen_  = false;

    // Erase the speed number and trip/rng row, but keep the gas arc.
    gfx->fillRect(kCx - 62, kCy - 36, 124, 72, COLOR_BG);  // speed block
    gfx->fillRect(kCx - 52, kCy + 50, 104, 40, COLOR_BG);  // trip/rng + bar
    // Erase the load arc quadrant (top-right).
    drawArc(kCx, kCy, 118, 114, -90.0f, 0.0f, COLOR_BG, false);
    // Also erase the KM/H label and TRIP/RNG labels.
    gfx->fillRect(kCx - 30, kCy + 24, 60, 14, COLOR_BG);   // KM/H
    gfx->fillRect(kCx - 52, kCy + 50, 104, 12, COLOR_BG);  // TRIP / RNG labels

    // Invalidate parked cache so every field redraws on first tick.
    last_park_hh_   = 0xFF;
    last_park_mm_   = 0xFF;
    last_park_temp_ = static_cast<int8_t>(telemetry::kWeatherTempUnknown - 1);

    if (log_) log_->println("[DISP] Parked screen ON");
  }

  // Transition: parked → driving
  if (!should_park && in_parked_screen_) {
    in_parked_screen_ = false;

    // Full wipe and restore speed-screen chrome.
    gfx->fillScreen(COLOR_BG);
    drawSpeedScreenChrome();

    // Force all dynamic elements to redraw.
    last_spd_ = -1;
    last_gas_ = -1.0f;
    last_lod_ = -1.0f;

    if (log_) log_->println("[DISP] Parked screen OFF — speed resumed");
  }

  updateBacklight(static_cast<uint8_t>(t.drive_mode), now_ms);

  // ── Smoothed sensor values (run every frame, both screens) ───────────────
  const float raw_gas  = t.fuel_level_pct / 100.0f;
  float load_raw = static_cast<float>(t.rpm) / 7000.0f;
  load_raw = constrain(load_raw, 0.0f, 1.0f);

  smooth_gas_ += (raw_gas  - smooth_gas_) * 0.1f;
  smooth_lod_ += (load_raw - smooth_lod_) * 0.3f;

  // ── Gas arc — always drawn (shared by both screens) ───────────────────────
  if (fabsf(smooth_gas_ - last_gas_) > 0.01f) {
    const float gas_sweep = smooth_gas_ * 90.0f;

    // Clear the whole quadrant then redraw.
    drawArc(kCx, kCy, 118, 114, 90.0f, 180.0f, COLOR_BG, false);
    drawArc(kCx, kCy, 118, 114, 90.0f, 90.0f + gas_sweep, COLOR_GAS_ARC, true);

    // Erase old tip label.
    if (last_gas_ >= 0.0f) {
      const float old_tip = (90.0f + last_gas_ * 90.0f) * DEG_TO_RAD;
      const int old_gx = kCx + static_cast<int>(cosf(old_tip) * 100.0f);
      const int old_gy = kCy + static_cast<int>(sinf(old_tip) * 100.0f);
      gfx->fillRect(old_gx - 16, old_gy - 9, 32, 18, COLOR_BG);
    }

    // Draw new tip label.
    const float tip_rad = (90.0f + gas_sweep) * DEG_TO_RAD;
    const int gx = kCx + static_cast<int>(cosf(tip_rad) * 100.0f);
    const int gy = kCy + static_cast<int>(sinf(tip_rad) * 100.0f);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(smooth_gas_ * 100.0f));
    renderCenteredText(gx, gy, 30, 16, buf, COLOR_WHITE, 1);

    last_gas_ = smooth_gas_;
  }

  // ── Parked screen content ─────────────────────────────────────────────────
  if (in_parked_screen_) {
    renderParkedScreen(t, now_ms);
    return;  // skip speed / trip / load arc
  }

  // ── Speed screen content ──────────────────────────────────────────────────

  // Trip odometer (local accumulation — OBD2 doesn't expose this directly).
  static float  trip_km     = 0.0f;
  static uint32_t last_trip_ms = 0;
  if (last_trip_ms == 0) last_trip_ms = now_ms;
  const float dt_h = static_cast<float>(now_ms - last_trip_ms) / 3600000.0f;
  trip_km      += t.speed_kph * dt_h;
  last_trip_ms  = now_ms;

  const int range_km = static_cast<int>(420.0f * smooth_gas_);

  char buf[32];

  // Sunset logic: use amber if it's past sunset or before 6 AM
  uint8_t hh, mm;
  getWallClockHHMM(hh, mm);
  uint16_t curr_min = (hh * 60) + mm;
  bool is_night = false;
  if (t.sunset_minute != 0xFFFF) {
    if (curr_min >= t.sunset_minute || hh < 6) is_night = true;
  } else {
    if (hh >= 18 || hh < 6) is_night = true; // Fallback 6pm-6am
  }
  uint16_t main_color = is_night ? COLOR_AMBER : COLOR_WHITE;

  // Speed number — only redraw when value changes.
  if (t.speed_kph != last_spd_ || is_night != last_night_mode_) {
    last_spd_ = t.speed_kph;
    last_night_mode_ = is_night;
    snprintf(buf, sizeof(buf), "%d", t.speed_kph);
    renderCenteredText(kCx, kCy, 120, 60, buf, main_color, 6);
  }

  // Trip / range row — redraw every frame (cheap, avoids stale values).
  snprintf(buf, sizeof(buf), "%.1f", trip_km);
  renderRightText(kCx - 10, kCy + 70, 48, 12, buf, main_color, 1);

  snprintf(buf, sizeof(buf), "%d", range_km);
  renderLeftText(kCx + 10, kCy + 70, 32, 12, buf, main_color, 1);

  // Range bar.
  gfx->fillRect(kCx - 30, kCy + 82, 60, 2, COLOR_DIM);
  gfx->fillRect(kCx - 30, kCy + 82,
                static_cast<int>(60.0f * smooth_gas_), 2, COLOR_RNG_VAL);

  // Load arc (top-right quadrant) — only redraw when value changes.
  if (fabsf(smooth_lod_ - last_lod_) > 0.01f) {
    const float lod_sweep = smooth_lod_ * 90.0f;
    drawArc(kCx, kCy, 118, 114, -90.0f, 0.0f, COLOR_BG, false);

    const uint8_t g_col = static_cast<uint8_t>(200 - smooth_lod_ * 150.0f);
    drawArc(kCx, kCy, 118, 114, -90.0f, -90.0f + lod_sweep,
            rgb565(255, g_col, 0), false);

    last_lod_ = smooth_lod_;
  }
}

// ── renderDebugScreen ─────────────────────────────────────────────────────────

void OrbDisplay::renderDebugScreen(const char *status, const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (!initialized_) return;
  if (now_ms - last_debug_render_ms_ < 500) return;  // 2 FPS, own timestamp
  last_debug_render_ms_ = now_ms;

  if (!in_debug_screen_) {
    gfx->fillScreen(COLOR_BG);
    in_debug_screen_  = true;
    in_no_signal_     = false;
    in_parked_screen_ = false;

    gfx->setTextColor(COLOR_RNG_VAL, COLOR_BG);
    gfx->setTextSize(2);
    gfx->setCursor(kCx - 60, 30);
    gfx->print("DEBUG INFO");
  }

  gfx->setTextColor(COLOR_WHITE, COLOR_BG);
  gfx->setTextSize(1);

  int y = 60;
  constexpr int kDy = 20;
  char buf[64];

  gfx->setCursor(20, y);
  snprintf(buf, sizeof(buf), "Status: %-15s", status);
  gfx->print(buf); y += kDy;

  gfx->setCursor(20, y);
  snprintf(buf, sizeof(buf), "Spd:%d RPM:%d F:%d%%", t.speed_kph, t.rpm, t.fuel_level_pct);
  gfx->print(buf); y += kDy;

  gfx->setCursor(20, y);
  const char* wifi_str = "OFF       ";
  if (t.wifi_connected) {
    wifi_str = "ON        ";
  } else if (t.weather_temp_c == telemetry::kWeatherTempUnknown) {
    wifi_str = "CONNECTING";
  }
  snprintf(buf, sizeof(buf), "Wi-Fi:  %s", wifi_str);
  gfx->print(buf); y += kDy;

  gfx->setCursor(20, y);
  if (t.weather_temp_c != telemetry::kWeatherTempUnknown) {
    snprintf(buf, sizeof(buf), "Wthr:   %d\xF7" "C, code %d", t.weather_temp_c, t.weather_code);
  } else {
    snprintf(buf, sizeof(buf), "Wthr:   WAIT           ");
  }
  gfx->print(buf); y += kDy;

  gfx->setCursor(20, y);
  snprintf(buf, sizeof(buf), "Pkts:   %-10u", t.sequence);
  gfx->print(buf);

  updateBacklight(0, now_ms);
}

}  // namespace orb
