#include "orb_display.h"

#include <Arduino_GFX_Library.h>
#include <math.h>

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

constexpr uint32_t kModeTransitionMs = 600;
constexpr float kBreathSpeed = 0.003f;

Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

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

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void OrbDisplay::begin(Print &log) {
  log_ = &log;

  pinMode(kPinBl, OUTPUT);
  analogWrite(kPinBl, 255);

  bus = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, -1);
  gfx = new Arduino_GC9A01(bus, kPinRst, 0, true);
  gfx->begin();  // Do not check return — can return false even when hw is fine.

  // Simple bring-up: fill screen blue-teal so we can confirm display is alive.
  gfx->fillScreen(theme::chill::moodRing());
  delay(400);
  gfx->fillScreen(BLACK);

  initialized_ = true;
  needs_full_redraw_ = true;
  log_->println("[DISP] GC9A01 ready");
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
    const float p = static_cast<float>(elapsed) / static_cast<float>(kModeTransitionMs);
    const float e = p < 0.5f ? 2.0f * p * p : 1.0f - 2.0f * (1.0f - p) * (1.0f - p);
    mode_blend_ = mode_blend_ + (target - mode_blend_) * e;
  }
  return mode_blend_;
}

void OrbDisplay::updateBacklight(uint8_t drive_mode, const PetState &pet, uint32_t now_ms) {
  if (now_ms - last_backlight_ms_ < 200) return;
  last_backlight_ms_ = now_ms;

  uint8_t target = 200;
  if (pet.isSleeping()) {
    target = 40;
  } else if (theme::isSport(drive_mode)) {
    target = 255;
  } else if (!pet.isInTrip()) {
    target = 150;
  }

  if (backlight_level_ < target)
    backlight_level_ = min(static_cast<uint8_t>(backlight_level_ + 8), target);
  else if (backlight_level_ > target)
    backlight_level_ = max(static_cast<uint8_t>(backlight_level_ - 4), target);

  analogWrite(kPinBl, backlight_level_);
}

// ─── No-signal state ─────────────────────────────────────────────────────────

void OrbDisplay::renderNoSignal(uint32_t now_ms) {
  if (!initialized_) return;
  // 100 ms — animation is slow (breath ~2 s, Zzz drift ~4 s); no need for 30 fps.
  if (now_ms - last_no_signal_render_ms_ < 100) return;
  last_no_signal_render_ms_ = now_ms;

  const uint16_t bg = theme::chill::bg();

  if (!in_no_signal_) {
    gfx->fillScreen(bg);
    in_no_signal_ = true;
  }

  breath_phase_ += kBreathSpeed * 33.0f;
  if (breath_phase_ > 6.2832f) breath_phase_ -= 6.2832f;

  const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(now_ms) * 0.0008f);

  // Ring — 60–100% of full moodRing colour, clearly visible.
  const uint16_t ring_color = blend565(bg, theme::chill::moodRing(), 0.6f + 0.4f * pulse);
  drawArc(kCx, kCy, 119, 114, 0, 360, ring_color);

  drawOrbFaceSleeping(now_ms);

  // "NO SIG" label.
  gfx->fillRect(kCx - 42, kCy + 42, 84, 20, bg);
  gfx->setTextColor(theme::chill::accent());
  gfx->setTextSize(2);
  gfx->setCursor(kCx - 36, kCy + 44);
  gfx->print("NO SIG");

  // Backlight fully lit.
  if (backlight_level_ != 200) {
    backlight_level_ = 200;
    analogWrite(kPinBl, backlight_level_);
  }

  needs_full_redraw_ = true;
}

// ─── Main render ─────────────────────────────────────────────────────────────

void OrbDisplay::render(const PetState &pet, const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (!initialized_) return;
  in_no_signal_ = false;

  // Throttle rendering to ~30 FPS to avoid frequent partial screen updates.
  if (now_ms - last_render_ms_ < 33) return;
  last_render_ms_ = now_ms;

  const uint8_t dm = static_cast<uint8_t>(t.drive_mode);
  const uint8_t mood = static_cast<uint8_t>(t.companion_mood);
  const PetStage stage = pet.stage();
  const DrivingDynamics &dyn = pet.dynamics();

  // Advance breathing animation.
  breath_phase_ += kBreathSpeed * 33.0f;
  if (breath_phase_ > 6.2832f) breath_phase_ -= 6.2832f;

  modeTransition(dm, now_ms);
  updateBacklight(dm, pet, now_ms);

  if (dm != last_drive_mode_ || stage != last_stage_) {
    needs_full_redraw_ = true;
  }

  if (needs_full_redraw_) {
    drawFullScene(pet, t, now_ms);
    last_drive_mode_ = dm;
    last_mood_ = mood;
    last_rpm_ = t.rpm;
    last_stage_ = stage;
    needs_full_redraw_ = false;
    return;
  }

  // Incremental updates.
  drawXpRing(pet, dm, t.rpm, now_ms);

  if (pet.isSleeping()) {
    drawOrbFaceSleeping(now_ms);
  } else {
    uint8_t face_mood;
    if (dyn.in_sport_zone)          face_mood = 5;
    else if (pet.happiness() > 80)  face_mood = 5;
    else if (pet.happiness() > 60)  face_mood = 3;
    else if (pet.happiness() > 40)  face_mood = 1;
    else if (pet.happiness() > 20)  face_mood = 0;
    else                            face_mood = 4;
    drawOrbFace(face_mood, dyn, now_ms);
  }

  if (pet.isReactionActive(now_ms)) {
    drawReaction(pet.currentReaction(), pet.reactionAge(now_ms), now_ms);
  }

  drawStatBars(pet.happiness(), pet.energy(), stage);

  last_mood_ = mood;
  last_rpm_ = t.rpm;
}

void OrbDisplay::drawFullScene(const PetState &pet, const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  const uint8_t dm = static_cast<uint8_t>(t.drive_mode);
  const uint16_t bg_color = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  gfx->fillScreen(bg_color);

  const DrivingDynamics &dyn = pet.dynamics();

  drawXpRing(pet, dm, t.rpm, now_ms);

  if (pet.isSleeping()) {
    drawOrbFaceSleeping(now_ms);
  } else {
    const DrivingDynamics &fs_dyn = pet.dynamics();
    uint8_t face_mood;
    if (fs_dyn.in_sport_zone)          face_mood = 5;
    else if (pet.happiness() > 80)     face_mood = 5;
    else if (pet.happiness() > 60)     face_mood = 3;
    else if (pet.happiness() > 40)     face_mood = 1;
    else if (pet.happiness() > 20)     face_mood = 0;
    else                               face_mood = 4;
    drawOrbFace(face_mood, fs_dyn, now_ms);
  }

  if (pet.isReactionActive(now_ms)) {
    drawReaction(pet.currentReaction(), pet.reactionAge(now_ms), now_ms);
  }

  drawStatBars(pet.happiness(), pet.energy(), pet.stage());
}

// ─── XP Ring ─────────────────────────────────────────────────────────────────

void OrbDisplay::drawXpRing(const PetState &pet, uint8_t drive_mode, int16_t rpm, uint32_t now_ms) {
  const float rpm_norm = constrain(static_cast<float>(rpm) / 6000.0f, 0.0f, 1.0f);
  const float pulse_speed = 0.002f + rpm_norm * 0.012f;
  const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(now_ms) * pulse_speed);

  const uint16_t base = blend565(theme::chill::moodRing(), theme::sport::moodRing(), mode_blend_);
  const uint16_t dim = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);

  const uint16_t track_color = blend565(dim, base, 0.08f + 0.12f * pulse);
  drawArc(kCx, kCy, 119, 114, 0, 360, track_color);

  const float progress = pet.levelProgress();
  if (progress > 0.005f) {
    const float fill_end = 270.0f + progress * 360.0f;
    for (float a = 270.0f; a <= fill_end && a <= 630.0f; a += 0.8f) {
      float t = (a - 270.0f) / 360.0f;
      uint16_t c = blend565(theme::arcStart(), theme::arcEnd(), t);
      c = blend565(c, base, 0.15f + 0.15f * pulse);
      float rad = a * DEG_TO_RAD;
      float cs = cosf(rad), sn = sinf(rad);
      for (int r = 114; r <= 119; ++r) {
        int x = kCx + static_cast<int>(cs * r);
        int y = kCy + static_cast<int>(sn * r);
        if (x >= 0 && x < kW && y >= 0 && y < kH) {
          gfx->drawPixel(x, y, c);
        }
      }
    }
  }
}

// ─── Orb Face - the whole display IS the face ─────────────────────────────

void OrbDisplay::drawOrbFace(uint8_t mood, const DrivingDynamics &dyn, uint32_t now_ms) {
  const uint16_t bg     = blend565(theme::chill::bg(),           theme::sport::bg(),           mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(),       theme::sport::accent(),       mode_blend_);
  const uint16_t strong = blend565(theme::chill::accentStrong(), theme::sport::accentStrong(), mode_blend_);

  // Breathing: subtle vertical drift.
  const float breath = 0.5f + 0.5f * sinf(breath_phase_);
  const int b_off = static_cast<int>(breath * 2.0f);

  // G-force lean: face tilts horizontally with lateral acceleration.
  const int lean_px = static_cast<int>(
      constrain(static_cast<float>(dyn.accel_mg) / 200.0f, -15.0f, 15.0f));

  // Face anchor slightly above center to leave room for stat bars.
  const int fx = kCx + lean_px;
  const int fy = kCy - 5 + b_off;

  const int eye_spacing = 40;
  const int eye_r       = 5;    // small dot
  const int eye_y       = fy - 18;
  const int lx          = fx - eye_spacing;
  const int rx          = fx + eye_spacing;
  const int mouth_y     = fy + 30;

  // Clear only the face interior zones that change, not the whole circle.
  // Eyes strip (covers b_off travel + dot height + lean + margin).
  gfx->fillRect(kCx - 100, eye_y - 15, 200, 30, bg);
  // Cheek strip.
  gfx->fillRect(kCx - 90, eye_y + 8, 180, 30, bg);
  // Mouth strip.
  gfx->fillRect(kCx - 50, mouth_y - 4, 100, 28, bg);

  // -- Eyes (small dots for all moods) --

  if (mood == 3) {
    // Happy: squinted — thin horizontal lines.
    gfx->fillRect(lx - eye_r - 2, eye_y - 1, (eye_r + 2) * 2, 3, accent);
    gfx->fillRect(rx - eye_r - 2, eye_y - 1, (eye_r + 2) * 2, 3, accent);

  } else if (mood == 4) {
    // Sad: small dots + droopy angled brows above.
    gfx->fillCircle(lx, eye_y, eye_r, accent);
    gfx->fillCircle(rx, eye_y, eye_r, accent);
    gfx->drawLine(lx - 10, eye_y - 12, lx + 10, eye_y - 8,  accent);
    gfx->drawLine(rx - 10, eye_y - 8,  rx + 10, eye_y - 12, accent);

  } else if (mood == 5) {
    // Excited: small dots with X cross.
    gfx->fillCircle(lx, eye_y, eye_r, accent);
    gfx->fillCircle(rx, eye_y, eye_r, accent);
    const int d = eye_r - 1;
    gfx->drawLine(lx - d, eye_y - d, lx + d, eye_y + d, bg);
    gfx->drawLine(lx + d, eye_y - d, lx - d, eye_y + d, bg);
    gfx->drawLine(rx - d, eye_y - d, rx + d, eye_y + d, bg);
    gfx->drawLine(rx + d, eye_y - d, rx - d, eye_y + d, bg);

  } else if (mood == 2) {
    // Alert: slightly larger dots with catchlights; periodic blink.
    const int alert_r = eye_r + 2;
    gfx->fillCircle(lx, eye_y, alert_r, accent);
    gfx->fillCircle(rx, eye_y, alert_r, accent);
    gfx->fillCircle(lx - 2, eye_y - 2, 2, strong);
    gfx->fillCircle(rx - 2, eye_y - 2, 2, strong);
    if ((now_ms / 300) % 7 == 0) {
      // Blink: replace dots with thin lines.
      gfx->fillCircle(lx, eye_y, alert_r, bg);
      gfx->fillCircle(rx, eye_y, alert_r, bg);
      gfx->drawFastHLine(lx - alert_r, eye_y, alert_r * 2, accent);
      gfx->drawFastHLine(rx - alert_r, eye_y, alert_r * 2, accent);
    }

  } else {
    // 0 = Neutral, 1 = Warm — small plain dots.
    gfx->fillCircle(lx, eye_y, eye_r, accent);
    gfx->fillCircle(rx, eye_y, eye_r, accent);
    if (mood == 1) {
      gfx->fillCircle(lx - 2, eye_y - 2, 2, strong);
      gfx->fillCircle(rx - 2, eye_y - 2, 2, strong);
    }
  }

  // -- Cheeks --
  if (mood == 1 || mood == 3 || mood == 5) {
    const uint16_t blush =
        blend565(bg, blend565(accent, theme::chill::moodRing(), 0.3f), 0.28f);
    gfx->fillCircle(fx - 64, eye_y + 22, 14, blush);
    gfx->fillCircle(fx + 64, eye_y + 22, 14, blush);
  }

  // -- Mouth --
  if (mood == 0) {
    gfx->drawFastHLine(fx - 22, mouth_y,     44, accent);
    gfx->drawFastHLine(fx - 22, mouth_y + 1, 44, accent);
  } else if (mood == 1) {
    for (int x = -28; x <= 28; ++x) {
      const int yo = static_cast<int>(12.0f * sinf(static_cast<float>(x + 28) * 3.14159f / 56.0f));
      gfx->drawPixel(fx + x, mouth_y + yo,     accent);
      gfx->drawPixel(fx + x, mouth_y + yo + 1, accent);
    }
  } else if (mood == 2) {
    gfx->fillCircle(fx, mouth_y + 8, 14, accent);
    gfx->fillCircle(fx, mouth_y + 8,  9, bg);
  } else if (mood == 3 || mood == 5) {
    for (int x = -36; x <= 36; ++x) {
      const int yo = static_cast<int>(18.0f * sinf(static_cast<float>(x + 36) * 3.14159f / 72.0f));
      gfx->drawPixel(fx + x, mouth_y + yo,     accent);
      gfx->drawPixel(fx + x, mouth_y + yo + 1, accent);
      gfx->drawPixel(fx + x, mouth_y + yo + 2, accent);
    }
  } else if (mood == 4) {
    for (int x = -28; x <= 28; ++x) {
      const int yo = static_cast<int>(
          -12.0f * sinf(static_cast<float>(x + 28) * 3.14159f / 56.0f));
      gfx->drawPixel(fx + x, mouth_y + 14 + yo, accent);
      gfx->drawPixel(fx + x, mouth_y + 15 + yo, accent);
    }
  }
}

// ─── Orb Face - sleeping ─────────────────────────────────────────────────────

void OrbDisplay::drawOrbFaceSleeping(uint32_t now_ms) {
  const uint16_t bg     = blend565(theme::chill::bg(),       theme::sport::bg(),       mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(),   theme::sport::accent(),   mode_blend_);

  const float breath = 0.5f + 0.5f * sinf(breath_phase_ * 0.4f);
  const int b_off = static_cast<int>(breath * 3.0f);

  const int eye_spacing = 40;
  const int lx = kCx - eye_spacing;
  const int rx = kCx + eye_spacing;

  // NO full-circle erase — that causes the flicker.
  // Clear only the tight strip around each animated element.

  // Eyes: b_off travels 0–3 px. Cover the full travel range + element height (5px) + margin.
  // Strip height = margin(2) + travel(3) + eye_height(5) + margin(2) = 12 px.
  const int eye_strip_top = kCy - 27;  // kCy - 23 (base) - 2 (margin) - 3 (max b_off) - 1
  gfx->fillRect(lx - 18, eye_strip_top, 36, 12, bg);
  gfx->fillRect(rx - 18, eye_strip_top, 36, 12, bg);
  const int eye_y = kCy - 23 + b_off;
  gfx->fillRect(lx - 16, eye_y - 2, 32, 5, accent);
  gfx->fillRect(rx - 16, eye_y - 2, 32, 5, accent);

  // Mouth: clears a strip covering full travel (3 px) + sine height (8 px) + margins.
  const int mouth_strip_top = kCy + 22;
  gfx->fillRect(kCx - 22, mouth_strip_top, 44, 18, bg);
  const int mouth_y = kCy + 25 + b_off;
  for (int x = -20; x <= 20; ++x) {
    const int yo = static_cast<int>(7.0f * sinf(static_cast<float>(x + 20) * 3.14159f / 40.0f));
    gfx->drawPixel(kCx + x, mouth_y + yo,     accent);
    gfx->drawPixel(kCx + x, mouth_y + yo + 1, accent);
    gfx->drawPixel(kCx + x, mouth_y + yo + 2, accent);
  }

  // Zzz: shifted inward so clear rect stays inside r=114 (ring inner boundary).
  // Clear rect x:138→182, y:40→98 — all corners < r=100 from center. Safe.
  const float z_drift = sinf(static_cast<float>(now_ms) * 0.0015f) * 4.0f;
  gfx->fillRect(kCx + 18, kCy - 80, 46, 58, bg);
  gfx->setTextColor(accent);
  gfx->setTextSize(2);
  gfx->setCursor(kCx + 20, kCy - 32 + static_cast<int>(z_drift));
  gfx->print("z");
  gfx->setCursor(kCx + 32, kCy - 52 + static_cast<int>(z_drift * 0.6f));
  gfx->print("Z");
  gfx->setCursor(kCx + 42, kCy - 70 + static_cast<int>(z_drift * 0.3f));
  gfx->print("Z");
}

// ─── Reaction Overlays ───────────────────────────────────────────────────────

void OrbDisplay::drawReaction(Reaction reaction, uint32_t age_ms, uint32_t now_ms) {
  const uint16_t accent = blend565(theme::chill::accent(), theme::sport::accent(), mode_blend_);
  const uint16_t strong = blend565(theme::chill::accentStrong(), theme::sport::accentStrong(), mode_blend_);
  const uint16_t warn = theme::warning();

  switch (reaction) {

    case Reaction::Startled: {
      if (age_ms < 300 || (age_ms / 150) % 2 == 0) {
        drawArc(kCx, kCy, 112, 108, 0, 360, warn);
      }
      gfx->setTextColor(warn);
      gfx->setTextSize(2);
      gfx->setCursor(kCx - 40, kCy - 50);
      gfx->print("!");
      gfx->setCursor(kCx + 30, kCy - 50);
      gfx->print("!");
      break;
    }

    case Reaction::Hearts: {
      const float phase = static_cast<float>(age_ms) / 1500.0f;
      for (int i = 0; i < 3; ++i) {
        float offset = static_cast<float>(i) * 0.33f;
        float y_rise = (phase + offset);
        if (y_rise > 1.0f) y_rise -= 1.0f;
        int hx = kCx - 30 + i * 30 + static_cast<int>(sinf(y_rise * 6.28f) * 8.0f);
        int hy = kCy + 30 - static_cast<int>(y_rise * 80.0f);
        if (hy > kCy - 60 && hy < kCy + 40) {
          gfx->fillCircle(hx - 2, hy, 3, strong);
          gfx->fillCircle(hx + 2, hy, 3, strong);
          gfx->fillTriangle(hx - 5, hy + 1, hx + 5, hy + 1, hx, hy + 7, strong);
        }
      }
      break;
    }

    case Reaction::Stars: {
      for (int i = 0; i < 4; ++i) {
        float angle = (static_cast<float>(now_ms) * 0.004f) + i * 1.5708f;
        int sx = kCx + static_cast<int>(cosf(angle) * 50.0f);
        int sy = kCy - 10 + static_cast<int>(sinf(angle) * 35.0f);
        if (sx > 4 && sx < kW - 4 && sy > 4 && sy < kH - 4) {
          gfx->drawPixel(sx, sy, strong);
          gfx->drawPixel(sx - 1, sy, accent);
          gfx->drawPixel(sx + 1, sy, accent);
          gfx->drawPixel(sx, sy - 1, accent);
          gfx->drawPixel(sx, sy + 1, accent);
          gfx->drawPixel(sx - 2, sy, strong);
          gfx->drawPixel(sx + 2, sy, strong);
          gfx->drawPixel(sx, sy - 2, strong);
          gfx->drawPixel(sx, sy + 2, strong);
        }
      }
      break;
    }

    case Reaction::Sleepy: {
      const uint16_t dim = blend565(theme::chill::accentDim(), theme::sport::accentDim(), mode_blend_);
      float drift = sinf(static_cast<float>(now_ms) * 0.002f) * 4.0f;
      gfx->setTextColor(dim);
      gfx->setTextSize(1);
      gfx->setCursor(kCx + 25, kCy - 40 + static_cast<int>(drift));
      gfx->print("z");
      gfx->setCursor(kCx + 35, kCy - 52 + static_cast<int>(drift * 0.6f));
      gfx->print("Z");
      gfx->setCursor(kCx + 42, kCy - 62 + static_cast<int>(drift * 0.3f));
      gfx->print("Z");
      break;
    }

    case Reaction::Celebrate: {
      const float phase = static_cast<float>(age_ms) / 2500.0f;
      for (int i = 0; i < 8; ++i) {
        float angle = static_cast<float>(i) * 0.7854f + phase * 3.14159f;
        float radius = phase * 70.0f;
        int px = kCx + static_cast<int>(cosf(angle) * radius);
        int py = kCy - 10 + static_cast<int>(sinf(angle) * radius * 0.7f);
        if (px > 2 && px < kW - 2 && py > 2 && py < kH - 2) {
          uint16_t c = (i % 2 == 0) ? strong : theme::sport::moodRing();
          gfx->fillRect(px - 1, py - 1, 3, 3, c);
        }
      }
      if (age_ms < 2000) {
        gfx->setTextColor(strong);
        gfx->setTextSize(1);
        gfx->setCursor(kCx - 20, kCy - 56);
        gfx->print("LVL UP!");
      }
      break;
    }

    case Reaction::Worried: {
      int dx = kCx + 30;
      int dy = kCy - 30 + static_cast<int>(sinf(static_cast<float>(now_ms) * 0.005f) * 3.0f);
      gfx->fillCircle(dx, dy, 3, theme::chill::accentAlt());
      gfx->fillTriangle(dx - 2, dy - 2, dx + 2, dy - 2, dx, dy - 7, theme::chill::accentAlt());
      break;
    }

    case Reaction::GForce: {
      // Pet braces — motion lines behind the body in lean direction.
      const uint16_t dim = blend565(theme::chill::accentDim(), theme::sport::accentDim(), mode_blend_);
      for (int i = 0; i < 4; ++i) {
        int lx = kCx - 45 + static_cast<int>(sinf(static_cast<float>(age_ms) * 0.01f + i) * 5.0f);
        int ly = kCy - 20 + i * 12;
        gfx->drawFastHLine(lx, ly, 8, dim);
      }
      // "G!" indicator.
      gfx->setTextColor(warn);
      gfx->setTextSize(1);
      gfx->setCursor(kCx + 35, kCy - 45);
      gfx->print("G!");
      break;
    }

    case Reaction::GearShift: {
      // Quick vertical bounce lines (gear shift jolt).
      for (int i = 0; i < 3; ++i) {
        int bx = kCx - 20 + i * 20;
        int len = 6 + (i % 2) * 4;
        int by = kCy + 40 - static_cast<int>(static_cast<float>(age_ms) * 0.03f);
        if (by > kCy + 10) {
          gfx->drawFastVLine(bx, by, len, accent);
        }
      }
      break;
    }

    case Reaction::SpeedCheer: {
      // Arms up + speed text flashing.
      if ((age_ms / 200) % 2 == 0) {
        gfx->setTextColor(strong);
        gfx->setTextSize(1);
        gfx->setCursor(kCx - 14, kCy - 58);
        gfx->print("WOOO!");
      }
      // Sparkle burst.
      for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(i) * 1.0472f + static_cast<float>(now_ms) * 0.005f;
        int sx = kCx + static_cast<int>(cosf(angle) * 55.0f);
        int sy = kCy - 5 + static_cast<int>(sinf(angle) * 40.0f);
        if (sx > 2 && sx < kW - 2 && sy > 2 && sy < kH - 2) {
          gfx->fillCircle(sx, sy, 2, strong);
        }
      }
      break;
    }

    case Reaction::Chilly: {
      // Shiver lines on both sides + snowflake dots.
      const uint16_t ice = theme::chill::accentAlt();
      for (int i = 0; i < 3; ++i) {
        int shiver = static_cast<int>(sinf(static_cast<float>(now_ms) * 0.02f + i) * 3.0f);
        gfx->drawFastHLine(kCx - 46 + shiver, kCy - 15 + i * 10, 6, ice);
        gfx->drawFastHLine(kCx + 40 - shiver, kCy - 15 + i * 10, 6, ice);
      }
      // Tiny snowflake dots floating down.
      for (int i = 0; i < 4; ++i) {
        int fx = kCx - 35 + i * 22 + static_cast<int>(sinf(static_cast<float>(now_ms) * 0.003f + i) * 5.0f);
        int fy = kCy - 55 + static_cast<int>((static_cast<float>(age_ms) * 0.04f)) % 80;
        if (fy > kCy - 60 && fy < kCy + 30) {
          gfx->drawPixel(fx, fy, ice);
          gfx->drawPixel(fx - 1, fy, ice);
          gfx->drawPixel(fx + 1, fy, ice);
          gfx->drawPixel(fx, fy - 1, ice);
          gfx->drawPixel(fx, fy + 1, ice);
        }
      }
      gfx->setTextColor(ice);
      gfx->setTextSize(1);
      gfx->setCursor(kCx - 10, kCy + 30);
      gfx->print("brr");
      break;
    }

    case Reaction::SmoothStop: {
      // Thumbs-up sparkle.
      gfx->setTextColor(strong);
      gfx->setTextSize(1);
      if ((age_ms / 250) % 2 == 0) {
        gfx->setCursor(kCx - 8, kCy - 55);
        gfx->print("+1");
      }
      // Radiating circles.
      int rad = static_cast<int>(static_cast<float>(age_ms) * 0.04f);
      if (rad < 50) {
        gfx->drawCircle(kCx, kCy - 10, rad, accent);
      }
      break;
    }

    case Reaction::Headbang: {
      // Head bobbing — vertical "music note" symbols rising.
      for (int i = 0; i < 3; ++i) {
        float drift = sinf(static_cast<float>(now_ms) * 0.006f + static_cast<float>(i) * 2.0f) * 6.0f;
        int nx = kCx + 30 + i * 12;
        int ny = kCy - 35 - static_cast<int>(static_cast<float>(age_ms) * 0.02f) + static_cast<int>(drift);
        if (ny > kCy - 70 && ny < kCy - 10 && nx < kW - 5) {
          gfx->fillCircle(nx, ny, 2, accent);
          gfx->drawFastVLine(nx + 2, ny - 6, 6, accent);
          gfx->drawPixel(nx + 3, ny - 6, accent);
        }
      }
      break;
    }

    case Reaction::None:
      break;
  }
}

// ─── Stat Bars ───────────────────────────────────────────────────────────────

void OrbDisplay::drawStatBars(uint8_t happiness, uint8_t energy, PetStage stage) {
  const uint16_t bg = blend565(theme::chill::bg(), theme::sport::bg(), mode_blend_);
  const uint16_t accent = blend565(theme::chill::accent(), theme::sport::accent(), mode_blend_);
  const uint16_t dim = blend565(theme::chill::accentDim(), theme::sport::accentDim(), mode_blend_);
  const uint16_t warn = theme::warning();

  const int bar_y = kCy + 56;
  const int bar_w = 50;
  const int bar_h = 4;

  gfx->fillRect(kCx - 52, bar_y - 4, 104, 30, bg);

  gfx->setTextColor(accent);
  gfx->setTextSize(1);
  gfx->setCursor(kCx - 52, bar_y - 2);
  gfx->print("<3");

  int h_fill = static_cast<int>(happiness / 100.0f * bar_w);
  gfx->fillRect(kCx - 36, bar_y, bar_w, bar_h, dim);
  uint16_t h_color = (happiness < 30) ? warn : accent;
  gfx->fillRect(kCx - 36, bar_y, h_fill, bar_h, h_color);

  gfx->setCursor(kCx - 52, bar_y + 10);
  gfx->print("zz");

  int e_fill = static_cast<int>(energy / 100.0f * bar_w);
  gfx->fillRect(kCx - 36, bar_y + 12, bar_w, bar_h, dim);
  uint16_t e_color = (energy < 20) ? warn : accent;
  gfx->fillRect(kCx - 36, bar_y + 12, e_fill, bar_h, e_color);

  const char *stage_labels[] = {"EGG", "BABY", "TEEN", "MAX"};
  uint8_t si = static_cast<uint8_t>(stage);
  if (si > 3) si = 3;
  gfx->setTextColor(dim);
  gfx->setCursor(kCx + 18, bar_y + 4);
  gfx->print(stage_labels[si]);
}

}  // namespace orb
