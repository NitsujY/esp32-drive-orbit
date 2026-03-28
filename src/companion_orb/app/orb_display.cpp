#include "orb_display.h"

#include <Arduino_GFX_Library.h>
#include <math.h>

#include "cyber_theme.h"

namespace orb {

namespace {

// GC9A01 pin mapping (from NitsujY/esp32carconsole reference).
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

const char *gearChar(telemetry::TransmissionGear gear) {
  switch (gear) {
    case telemetry::TransmissionGear::Park:   return "P";
    case telemetry::TransmissionGear::First:  return "1";
    case telemetry::TransmissionGear::Second: return "2";
    case telemetry::TransmissionGear::Third:  return "3";
    case telemetry::TransmissionGear::Fourth: return "4";
    case telemetry::TransmissionGear::Fifth:  return "5";
    case telemetry::TransmissionGear::Sixth:  return "6";
  }
  return "?";
}

// Draw a thick arc (pixel-by-pixel for simplicity on 240x240).
void drawArc(int cx, int cy, int r_outer, int r_inner,
             float start_deg, float end_deg, uint16_t color) {
  for (float a = start_deg; a <= end_deg; a += 0.8f) {
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

}  // namespace

void OrbDisplay::begin(Print &log) {
  log_ = &log;

  bus = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, -1);
  gfx = new Arduino_GC9A01(bus, kPinRst, 0, true);

  gfx->begin();
  gfx->fillScreen(BLACK);

  pinMode(kPinBl, OUTPUT);
  digitalWrite(kPinBl, HIGH);

  initialized_ = true;
  needs_full_redraw_ = true;
  log_->println("[DISP] GC9A01 240x240 cyber orb ready");
}

void OrbDisplay::render(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms) {
  if (!initialized_) return;

  const uint8_t dm = static_cast<uint8_t>(telemetry.drive_mode);
  const uint8_t mood = static_cast<uint8_t>(telemetry.companion_mood);
  const uint8_t gear = static_cast<uint8_t>(telemetry.gear);

  // Compute coaching score: penalize harsh speed changes.
  if (now_ms - last_score_update_ms_ > 500) {
    const int16_t delta = abs(telemetry.speed_kph - prev_speed_);
    if (delta > 15) {
      coaching_score_ = coaching_score_ > 5 ? coaching_score_ - 5 : 0;
    } else if (delta < 3 && coaching_score_ < 99) {
      ++coaching_score_;
    }
    prev_speed_ = telemetry.speed_kph;
    last_score_update_ms_ = now_ms;
  }

  // Mode change triggers full redraw.
  if (dm != last_drive_mode_) {
    needs_full_redraw_ = true;
  }

  if (needs_full_redraw_) {
    drawFullScene(telemetry, now_ms);
    last_drive_mode_ = dm;
    last_mood_ = mood;
    last_gear_ = gear;
    last_rpm_ = telemetry.rpm;
    last_score_ = coaching_score_;
    needs_full_redraw_ = false;
    return;
  }

  // Incremental updates.
  if (mood != last_mood_) {
    drawMoodRing(dm);
    drawFace(dm, mood, now_ms);
    last_mood_ = mood;
  }

  if (theme::isSport(dm)) {
    // Sport: show gear + RPM sweep.
    if (gear != last_gear_ || telemetry.rpm != last_rpm_) {
      drawGearRpm(telemetry.gear, telemetry.rpm, dm);
      last_gear_ = gear;
      last_rpm_ = telemetry.rpm;
    }
  } else {
    // Chill: show coaching score.
    if (coaching_score_ != last_score_) {
      drawCoachingScore(coaching_score_, dm);
      last_score_ = coaching_score_;
    }
  }
}

void OrbDisplay::drawFullScene(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms) {
  const uint8_t dm = static_cast<uint8_t>(telemetry.drive_mode);
  gfx->fillScreen(theme::bg(dm));

  drawMoodRing(dm);
  drawFace(dm, static_cast<uint8_t>(telemetry.companion_mood), now_ms);

  if (theme::isSport(dm)) {
    drawGearRpm(telemetry.gear, telemetry.rpm, dm);
  } else {
    drawCoachingScore(coaching_score_, dm);
  }
}

void OrbDisplay::drawMoodRing(uint8_t drive_mode) {
  const uint16_t color = theme::moodRing(drive_mode);
  drawArc(kCx, kCy, 119, 113, 0, 360, color);
}

void OrbDisplay::drawFace(uint8_t drive_mode, uint8_t mood, uint32_t now_ms) {
  const uint16_t bg = theme::bg(drive_mode);
  const uint16_t accent = theme::accent(drive_mode);

  // Clear face area.
  gfx->fillCircle(kCx, kCy - 20, 40, bg);

  // Eyes: two dots that change with mood.
  const int eye_y = kCy - 28;
  const int eye_spacing = theme::isSport(drive_mode) ? 22 : 18;
  const int eye_r = theme::isSport(drive_mode) ? 6 : 5;

  gfx->fillCircle(kCx - eye_spacing, eye_y, eye_r, accent);
  gfx->fillCircle(kCx + eye_spacing, eye_y, eye_r, accent);

  // Mouth: changes with mood.
  const int mouth_y = kCy - 4;
  if (mood == 0) {
    // Idle: small horizontal line (neutral).
    gfx->drawFastHLine(kCx - 8, mouth_y, 16, accent);
  } else if (mood == 1) {
    // Warm: slight smile arc.
    for (int x = -12; x <= 12; ++x) {
      int y_off = static_cast<int>(2.0f * sinf(static_cast<float>(x + 12) * 3.14159f / 24.0f));
      gfx->drawPixel(kCx + x, mouth_y + y_off, accent);
    }
  } else {
    // Alert: wide open (sport excitement).
    gfx->fillCircle(kCx, mouth_y + 2, 6, accent);
    gfx->fillCircle(kCx, mouth_y + 2, 3, bg);

    // Animated blink in sport: eyes narrow periodically.
    if ((now_ms / 300) % 4 == 0) {
      gfx->fillCircle(kCx - eye_spacing, eye_y, eye_r, bg);
      gfx->fillCircle(kCx + eye_spacing, eye_y, eye_r, bg);
      gfx->drawFastHLine(kCx - eye_spacing - eye_r, eye_y, eye_r * 2, accent);
      gfx->drawFastHLine(kCx + eye_spacing - eye_r, eye_y, eye_r * 2, accent);
    }
  }
}

void OrbDisplay::drawCoachingScore(uint8_t score, uint8_t drive_mode) {
  const uint16_t bg = theme::bg(drive_mode);
  const uint16_t accent = theme::accent(drive_mode);
  const uint16_t dim = theme::accentDim(drive_mode);

  // Clear score area.
  gfx->fillRect(kCx - 40, kCy + 30, 80, 50, bg);

  // Score number.
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", score);
  gfx->setTextColor(accent);
  gfx->setTextSize(3);
  gfx->setCursor(kCx - 18, kCy + 32);
  gfx->print(buf);

  // Label.
  gfx->setTextColor(dim);
  gfx->setTextSize(1);
  gfx->setCursor(kCx - 18, kCy + 62);
  gfx->print("smooth");
}

void OrbDisplay::drawGearRpm(telemetry::TransmissionGear gear, int16_t rpm, uint8_t drive_mode) {
  const uint16_t bg = theme::bg(drive_mode);
  const uint16_t accent = theme::accent(drive_mode);

  // Clear gear area.
  gfx->fillRect(kCx - 30, kCy + 25, 60, 45, bg);

  // Big gear character.
  gfx->setTextColor(accent);
  gfx->setTextSize(4);
  gfx->setCursor(kCx - 12, kCy + 28);
  gfx->print(gearChar(gear));

  // RPM sweep arc (bottom half, 200-340 degrees).
  const float rpm_norm = constrain(static_cast<float>(rpm) / 7000.0f, 0.0f, 1.0f);
  const float sweep_end = 200.0f + rpm_norm * 140.0f;

  // Clear previous arc.
  drawArc(kCx, kCy, 108, 103, 200, 340, bg);

  // Draw active RPM arc.
  uint16_t arc_color = rpm > 5500 ? theme::sport::accent() : accent;
  drawArc(kCx, kCy, 108, 103, 200, sweep_end, arc_color);

  // Track (inactive portion).
  if (sweep_end < 340.0f) {
    drawArc(kCx, kCy, 108, 105, sweep_end, 340, theme::track(drive_mode));
  }
}

}  // namespace orb
