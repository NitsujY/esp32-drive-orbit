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

constexpr uint32_t kModeTransitionMs = 600;   // fade duration
constexpr uint32_t kSleepAfterMs = 20000;     // sleep after 20s parked
constexpr uint8_t kLowFuelThreshold = 15;
constexpr float kBreathSpeed = 0.0025f;       // radians per ms

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

// Blend two 565 colors by factor t (0.0 = a, 1.0 = b).
uint16_t blend565(uint16_t a, uint16_t b, float t) {
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint8_t rr = ar + static_cast<uint8_t>((br - ar) * t);
  uint8_t rg = ag + static_cast<uint8_t>((bg - ag) * t);
  uint8_t rb = ab + static_cast<uint8_t>((bb - ab) * t);
  return (rr << 11) | (rg << 5) | rb;
}

}  // namespace

void OrbDisplay::begin(Print &log) {
  log_ = &log;

  bus = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, -1);
  gfx = new Arduino_GC9A01(bus, kPinRst, 0, true);

  gfx->begin();
  gfx->fillScreen(BLACK);

  pinMode(kPinBl, OUTPUT);
  analogWrite(kPinBl, 255);

  initialized_ = true;
  needs_full_redraw_ = true;
  log_->println("[DISP] GC9A01 240x240 cyber orb ready");
}

float OrbDisplay::modeTransition(uint8_t target_dm, uint32_t now_ms) {
  const float target = theme::isSport(target_dm) ? 1.0f : 0.0f;

  if (target_drive_mode_ != target_dm) {
    target_drive_mode_ = target_dm;
    mode_transition_start_ms_ = now_ms;
  }

  const uint32_t elapsed = now_ms - mode_transition_start_ms_;
  if (elapsed >= kModeTransitionMs) {
    mode_blend_ = target;
  } else {
    const float progress = static_cast<float>(elapsed) / static_cast<float>(kModeTransitionMs);
    // Ease in-out.
    const float eased = progress < 0.5f
        ? 2.0f * progress * progress
        : 1.0f - 2.0f * (1.0f - progress) * (1.0f - progress);
    mode_blend_ = mode_blend_ + (target - mode_blend_) * eased;
  }

  return mode_blend_;
}

void OrbDisplay::updateBacklight(uint8_t drive_mode, int16_t speed_kph, uint32_t now_ms) {
  if (now_ms - last_backlight_ms_ < 200) return;
  last_backlight_ms_ = now_ms;

  // Target brightness: dim when sleeping, medium in chill, full in sport.
  uint8_t target = 200;
  if (sleeping_) {
    target = 30;
  } else if (theme::isSport(drive_mode)) {
    target = 255;
  } else if (speed_kph == 0) {
    target = 140;  // parked but awake
  }

  // Smooth ramp.
  if (backlight_level_ < target) {
    backlight_level_ = min(static_cast<uint8_t>(backlight_level_ + 8), target);
  } else if (backlight_level_ > target) {
    backlight_level_ = max(static_cast<uint8_t>(backlight_level_ - 4), target);
  }

  analogWrite(kPinBl, backlight_level_);
}

void OrbDisplay::render(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms) {
  if (!initialized_) return;

  const uint8_t dm = static_cast<uint8_t>(telemetry.drive_mode);
  const uint8_t mood = static_cast<uint8_t>(telemetry.companion_mood);
  const uint8_t gear = static_cast<uint8_t>(telemetry.gear);
  const bool low_fuel = telemetry.fuel_level_pct < kLowFuelThreshold && telemetry.fuel_level_pct > 0;

  // Sleep detection: no motion for kSleepAfterMs.
  if (telemetry.speed_kph > 0 || telemetry.rpm > 100) {
    last_motion_ms_ = now_ms;
    if (sleeping_) {
      sleeping_ = false;
      needs_full_redraw_ = true;
    }
  } else if (!sleeping_ && last_motion_ms_ > 0 && (now_ms - last_motion_ms_ > kSleepAfterMs)) {
    sleeping_ = true;
    needs_full_redraw_ = true;
  }

  // Advance breathing animation.
  breath_phase_ += kBreathSpeed * 33.0f;  // ~33ms per frame
  if (breath_phase_ > 6.2832f) breath_phase_ -= 6.2832f;

  // Coaching score update.
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

  // Smooth mode transition.
  modeTransition(dm, now_ms);

  // Backlight management.
  updateBacklight(dm, telemetry.speed_kph, now_ms);

  // Mode change or fuel alert change triggers full redraw.
  if (dm != last_drive_mode_ || low_fuel != last_low_fuel_) {
    needs_full_redraw_ = true;
  }

  if (needs_full_redraw_) {
    drawFullScene(telemetry, now_ms);
    last_drive_mode_ = dm;
    last_mood_ = mood;
    last_gear_ = gear;
    last_rpm_ = telemetry.rpm;
    last_score_ = coaching_score_;
    last_fuel_pct_ = telemetry.fuel_level_pct;
    last_low_fuel_ = low_fuel;
    needs_full_redraw_ = false;
    return;
  }

  // Incremental updates.
  bool face_dirty = (mood != last_mood_);

  // Mood ring pulses with RPM — always update.
  drawMoodRing(dm, telemetry.rpm, now_ms);

  if (sleeping_) {
    // Only redraw sleep face occasionally for breathing.
    drawSleepFace(dm);
  } else if (low_fuel) {
    drawLowFuelFace(dm, now_ms);
  } else {
    if (face_dirty || true) {  // always redraw face for breathing
      drawFace(dm, mood, telemetry.rpm, now_ms);
      last_mood_ = mood;
    }
  }

  if (theme::isSport(dm)) {
    if (gear != last_gear_ || telemetry.rpm != last_rpm_) {
      drawGearRpm(telemetry.gear, telemetry.rpm, dm);
      last_gear_ = gear;
      last_rpm_ = telemetry.rpm;
    }
  } else {
    if (coaching_score_ != last_score_) {
      drawCoachingScore(coaching_score_, dm);
      last_score_ = coaching_score_;
    }
  }
}

void OrbDisplay::drawFullScene(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms) {
  const uint8_t dm = static_cast<uint8_t>(telemetry.drive_mode);
  const bool low_fuel = telemetry.fuel_level_pct < kLowFuelThreshold && telemetry.fuel_level_pct > 0;

  // Blended background during transition.
  const uint16_t bg_color = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  gfx->fillScreen(bg_color);

  drawMoodRing(dm, telemetry.rpm, now_ms);

  if (sleeping_) {
    drawSleepFace(dm);
  } else if (low_fuel) {
    drawLowFuelFace(dm, now_ms);
  } else {
    drawFace(dm, static_cast<uint8_t>(telemetry.companion_mood), telemetry.rpm, now_ms);
  }

  if (theme::isSport(dm)) {
    drawGearRpm(telemetry.gear, telemetry.rpm, dm);
  } else if (!sleeping_) {
    drawCoachingScore(coaching_score_, dm);
  }
}

void OrbDisplay::drawMoodRing(uint8_t drive_mode, int16_t rpm, uint32_t now_ms) {
  // Pulse rate synced to RPM: faster heartbeat at higher revs.
  // Base period ~2s at idle, ~0.3s at 6000 RPM.
  const float rpm_norm = constrain(static_cast<float>(rpm) / 6000.0f, 0.0f, 1.0f);
  const float pulse_speed = 0.002f + rpm_norm * 0.018f;  // radians per ms
  const float pulse_phase = static_cast<float>(now_ms) * pulse_speed;
  const float pulse = 0.5f + 0.5f * sinf(pulse_phase);  // 0..1

  // Blend mood ring color between chill and sport based on transition.
  const uint16_t base_color = blend565(theme::chill::moodRing(), theme::sport::moodRing(), mode_blend_);
  const uint16_t dim_color = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);

  // Pulse between dim and full brightness.
  const uint16_t ring_color = blend565(dim_color, base_color, 0.3f + 0.7f * pulse);

  drawArc(kCx, kCy, 119, 113, 0, 360, ring_color);
}

void OrbDisplay::drawFace(uint8_t drive_mode, uint8_t mood, int16_t rpm, uint32_t now_ms) {
  const uint16_t bg = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(), theme::sport::accent(), mode_blend_);

  // Breathing: eye size oscillates gently.
  const float breath = 0.5f + 0.5f * sinf(breath_phase_);  // 0..1
  const int breath_offset = static_cast<int>(breath * 2.0f);  // 0-2 pixel

  // Clear face area.
  gfx->fillCircle(kCx, kCy - 20, 42, bg);

  // Eye tracking: eyes shift horizontally based on RPM direction.
  // Positive RPM change = eyes look right, negative = left.
  const int rpm_delta = rpm - last_rpm_;
  int eye_shift = 0;
  if (rpm_delta > 50) eye_shift = 2;
  else if (rpm_delta < -50) eye_shift = -2;

  const int eye_y = kCy - 28 + breath_offset;
  const int eye_spacing = theme::isSport(drive_mode) ? 22 : 18;
  const int base_eye_r = theme::isSport(drive_mode) ? 6 : 5;
  const int eye_r = base_eye_r + (breath_offset > 1 ? 1 : 0);

  gfx->fillCircle(kCx - eye_spacing + eye_shift, eye_y, eye_r, accent);
  gfx->fillCircle(kCx + eye_spacing + eye_shift, eye_y, eye_r, accent);

  // Mouth.
  const int mouth_y = kCy - 4 + breath_offset;
  if (mood == 0) {
    // Idle: neutral line.
    gfx->drawFastHLine(kCx - 8, mouth_y, 16, accent);
  } else if (mood == 1) {
    // Warm: slight smile arc.
    for (int x = -12; x <= 12; ++x) {
      int y_off = static_cast<int>(2.0f * sinf(static_cast<float>(x + 12) * 3.14159f / 24.0f));
      gfx->drawPixel(kCx + x, mouth_y + y_off, accent);
    }
  } else if (mood == 2) {
    // Alert: open mouth + blink.
    gfx->fillCircle(kCx, mouth_y + 2, 6, accent);
    gfx->fillCircle(kCx, mouth_y + 2, 3, bg);
    if ((now_ms / 300) % 4 == 0) {
      gfx->fillCircle(kCx - eye_spacing + eye_shift, eye_y, eye_r, bg);
      gfx->fillCircle(kCx + eye_spacing + eye_shift, eye_y, eye_r, bg);
      gfx->drawFastHLine(kCx - eye_spacing - eye_r + eye_shift, eye_y, eye_r * 2, accent);
      gfx->drawFastHLine(kCx + eye_spacing - eye_r + eye_shift, eye_y, eye_r * 2, accent);
    }
  } else if (mood == 3) {
    // Happy: wide smile + squinted arc eyes.
    gfx->fillCircle(kCx - eye_spacing + eye_shift, eye_y, eye_r, bg);
    gfx->fillCircle(kCx + eye_spacing + eye_shift, eye_y, eye_r, bg);
    for (int x = -eye_r; x <= eye_r; ++x) {
      int yo = static_cast<int>(-1.5f * sinf(static_cast<float>(x + eye_r) * 3.14159f / (eye_r * 2.0f)));
      gfx->drawPixel(kCx - eye_spacing + eye_shift + x, eye_y + yo, accent);
      gfx->drawPixel(kCx + eye_spacing + eye_shift + x, eye_y + yo, accent);
    }
    for (int x = -16; x <= 16; ++x) {
      int y_off = static_cast<int>(4.0f * sinf(static_cast<float>(x + 16) * 3.14159f / 32.0f));
      gfx->drawPixel(kCx + x, mouth_y + y_off, accent);
      gfx->drawPixel(kCx + x, mouth_y + y_off + 1, accent);
    }
  } else if (mood == 4) {
    // Sad: droopy eyes + frown.
    gfx->drawLine(kCx - eye_spacing - eye_r, eye_y - 4, kCx - eye_spacing + eye_r, eye_y - 2, accent);
    gfx->drawLine(kCx + eye_spacing - eye_r, eye_y - 2, kCx + eye_spacing + eye_r, eye_y - 4, accent);
    for (int x = -12; x <= 12; ++x) {
      int y_off = static_cast<int>(-2.0f * sinf(static_cast<float>(x + 12) * 3.14159f / 24.0f));
      gfx->drawPixel(kCx + x, mouth_y + 4 + y_off, accent);
    }
  } else if (mood == 5) {
    // Excited: star eyes + big grin.
    gfx->fillCircle(kCx - eye_spacing + eye_shift, eye_y, eye_r, bg);
    gfx->fillCircle(kCx + eye_spacing + eye_shift, eye_y, eye_r, bg);
    for (int i = -eye_r; i <= eye_r; ++i) {
      gfx->drawPixel(kCx - eye_spacing + eye_shift + i, eye_y, accent);
      gfx->drawPixel(kCx - eye_spacing + eye_shift, eye_y + i, accent);
      gfx->drawPixel(kCx + eye_spacing + eye_shift + i, eye_y, accent);
      gfx->drawPixel(kCx + eye_spacing + eye_shift, eye_y + i, accent);
    }
    gfx->fillCircle(kCx, mouth_y + 2, 8, accent);
    gfx->fillCircle(kCx, mouth_y + 2, 5, bg);
    gfx->fillRect(kCx - 9, mouth_y - 4, 18, 6, bg);
  }
}

void OrbDisplay::drawLowFuelFace(uint8_t drive_mode, uint32_t now_ms) {
  const uint16_t bg = theme::bg(drive_mode);
  const uint16_t warn = theme::warning();

  // Clear face area.
  gfx->fillCircle(kCx, kCy - 20, 42, bg);

  // Worried eyes: angled eyebrows + smaller eyes.
  const int eye_y = kCy - 26;
  gfx->fillCircle(kCx - 18, eye_y, 4, warn);
  gfx->fillCircle(kCx + 18, eye_y, 4, warn);

  // Worried eyebrows (angled lines above eyes).
  gfx->drawLine(kCx - 26, eye_y - 8, kCx - 12, eye_y - 12, warn);
  gfx->drawLine(kCx + 12, eye_y - 12, kCx + 26, eye_y - 8, warn);

  // Wavy worried mouth.
  const int mouth_y = kCy - 2;
  for (int x = -14; x <= 14; ++x) {
    int y_off = static_cast<int>(2.0f * sinf(static_cast<float>(x) * 0.5f + static_cast<float>(now_ms) * 0.003f));
    gfx->drawPixel(kCx + x, mouth_y + y_off, warn);
  }

  // "LOW FUEL" label below face.
  gfx->fillRect(kCx - 35, kCy + 30, 70, 20, bg);
  gfx->setTextColor(warn);
  gfx->setTextSize(1);
  gfx->setCursor(kCx - 28, kCy + 34);
  gfx->print("LOW FUEL");
}

void OrbDisplay::drawSleepFace(uint8_t drive_mode) {
  const uint16_t bg = theme::bg(drive_mode);
  const uint16_t dim = theme::accentDim(drive_mode);

  // Clear face area.
  gfx->fillCircle(kCx, kCy - 10, 42, bg);

  // Closed eyes: just horizontal lines.
  const float breath = 0.5f + 0.5f * sinf(breath_phase_ * 0.4f);  // slow breathing
  const int y_shift = static_cast<int>(breath * 2.0f);

  const int eye_y = kCy - 20 + y_shift;
  gfx->drawFastHLine(kCx - 24, eye_y, 12, dim);
  gfx->drawFastHLine(kCx + 12, eye_y, 12, dim);

  // Zzz.
  gfx->setTextColor(dim);
  gfx->setTextSize(1);
  gfx->setCursor(kCx + 20, kCy - 38 + y_shift);
  gfx->print("z");
  gfx->setCursor(kCx + 28, kCy - 46 + y_shift);
  gfx->print("z");
}

void OrbDisplay::drawCoachingScore(uint8_t score, uint8_t drive_mode) {
  const uint16_t bg = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(), theme::sport::accent(), mode_blend_);
  const uint16_t dim = blend565(theme::chill::accentDim(), theme::sport::accentDim(), mode_blend_);

  gfx->fillRect(kCx - 40, kCy + 30, 80, 50, bg);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", score);
  gfx->setTextColor(accent);
  gfx->setTextSize(3);
  gfx->setCursor(kCx - 18, kCy + 32);
  gfx->print(buf);

  gfx->setTextColor(dim);
  gfx->setTextSize(1);
  gfx->setCursor(kCx - 18, kCy + 62);
  gfx->print("smooth");
}

void OrbDisplay::drawGearRpm(telemetry::TransmissionGear gear, int16_t rpm, uint8_t drive_mode) {
  const uint16_t bg = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(), theme::sport::accent(), mode_blend_);

  gfx->fillRect(kCx - 30, kCy + 25, 60, 45, bg);

  gfx->setTextColor(accent);
  gfx->setTextSize(4);
  gfx->setCursor(kCx - 12, kCy + 28);
  gfx->print(gearChar(gear));

  const float rpm_norm = constrain(static_cast<float>(rpm) / 7000.0f, 0.0f, 1.0f);
  const float sweep_end = 200.0f + rpm_norm * 140.0f;

  drawArc(kCx, kCy, 108, 103, 200, 340, bg);

  uint16_t arc_color = rpm > 5500 ? theme::sport::accent() : accent;
  drawArc(kCx, kCy, 108, 103, 200, sweep_end, arc_color);

  if (sweep_end < 340.0f) {
    drawArc(kCx, kCy, 108, 105, sweep_end, 340, theme::track(drive_mode));
  }
}

}  // namespace orb
