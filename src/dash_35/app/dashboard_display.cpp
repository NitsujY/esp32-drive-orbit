#include "dashboard_display.h"

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <TCA9554.h>
#include <Wire.h>

#include <math.h>
#include <stdio.h>

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
constexpr uint32_t kRenderIntervalMs = 33;
constexpr int16_t kScreenWidth = 480;
constexpr int16_t kClusterX = 0;
constexpr int16_t kClusterY = 56;
constexpr int16_t kClusterWidth = 480;
constexpr int16_t kClusterHeight = 196;
constexpr int16_t kClusterCenterX = kClusterWidth / 2;
constexpr int16_t kClusterCenterY = 186;
constexpr int16_t kArcOuterRadius = 188;
constexpr int16_t kArcTrackRadius = 172;
constexpr int16_t kArcTrackThickness = 18;
constexpr int16_t kArcInnerRadius = kArcTrackRadius - kArcTrackThickness;
constexpr int16_t kArcCapRadius = kArcTrackThickness / 2;
constexpr int16_t kArcOutlineCapRadius = 2;
constexpr float kArcOutlineStepDegrees = 0.4f;
constexpr float kArcBandStepDegrees = 0.32f;
constexpr float kArcGradientSegmentDegrees = 1.0f;
constexpr int16_t kBottomCardY = 258;
constexpr int16_t kBottomCardWidth = 110;
constexpr int16_t kBottomCardHeight = 50;
constexpr int16_t kBottomCardGap = 10;
constexpr int16_t kBottomCardX0 = 5;
constexpr int16_t kRpmMax = 5000;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect kScreenLabelRegion{18, 50, 130, 18};
constexpr Rect kModePillRegion{380, 18, 84, 24};
constexpr Rect kWelcomeRegion{140, 16, 200, 28};
constexpr Rect kHeroNoteRegion{166, 226, 148, 14};

TCA9554 io_expander(kIoExpanderAddress);
Arduino_DataBus *display_bus =
    new Arduino_ESP32SPI(kDisplayDcPin, kDisplayCsPin, kDisplaySckPin, kDisplayMosiPin,
                         kDisplayMisoPin);
Arduino_GFX *gfx =
    new Arduino_ST7796(display_bus, GFX_NOT_DEFINED, 0, true /* ips */);
Arduino_Canvas *cluster_canvas =
  new Arduino_Canvas(kClusterWidth, kClusterHeight, gfx, kClusterX, kClusterY, 0);

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t lerpColor565(uint16_t from, uint16_t to, float t) {
  const float clamped = constrain(t, 0.0f, 1.0f);
  const uint8_t from_r = static_cast<uint8_t>((from >> 11) & 0x1F);
  const uint8_t from_g = static_cast<uint8_t>((from >> 5) & 0x3F);
  const uint8_t from_b = static_cast<uint8_t>(from & 0x1F);
  const uint8_t to_r = static_cast<uint8_t>((to >> 11) & 0x1F);
  const uint8_t to_g = static_cast<uint8_t>((to >> 5) & 0x3F);
  const uint8_t to_b = static_cast<uint8_t>(to & 0x1F);

  const uint8_t r = static_cast<uint8_t>(from_r + (to_r - from_r) * clamped);
  const uint8_t g = static_cast<uint8_t>(from_g + (to_g - from_g) * clamped);
  const uint8_t b = static_cast<uint8_t>(from_b + (to_b - from_b) * clamped);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

uint16_t backgroundColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(21, 8, 9) : color565(7, 18, 24);
}

uint16_t panelColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(34, 11, 12) : color565(8, 20, 27);
}

uint16_t lineColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(91, 48, 45) : color565(42, 74, 84);
}

uint16_t accentColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(255, 126, 88) : color565(125, 249, 198);
}

uint16_t accentStrongColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(255, 209, 190)
                                             : color565(221, 255, 243);
}

uint16_t mutedColor() {
  return color565(142, 170, 183);
}

uint16_t textColor() {
  return color565(239, 251, 255);
}

uint16_t trackArcColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(84, 56, 56) : color565(70, 92, 112);
}

const char *screenTitle(DashboardScreen screen) {
  switch (screen) {
    case DashboardScreen::Boot:
      return "Boot";
    case DashboardScreen::Welcome:
      return "Welcome";
    case DashboardScreen::Dashboard:
      return "Dashboard";
    case DashboardScreen::Trip:
      return "Trip";
    case DashboardScreen::Health:
      return "Health";
    case DashboardScreen::Sport:
      return "Sport";
  }

  return "Dashboard";
}

const char *modeLabel(telemetry::DriveMode mode) {
  switch (mode) {
    case telemetry::DriveMode::Calm:
      return "Calm";
    case telemetry::DriveMode::Cruise:
      return "Cruise";
    case telemetry::DriveMode::Sport:
      return "Sport";
  }

  return "Cruise";
}

const char *gearLabel(telemetry::TransmissionGear gear) {
  switch (gear) {
    case telemetry::TransmissionGear::First:
      return "D1";
    case telemetry::TransmissionGear::Second:
      return "D2";
    case telemetry::TransmissionGear::Third:
      return "D3";
    case telemetry::TransmissionGear::Fourth:
      return "D4";
    case telemetry::TransmissionGear::Fifth:
      return "D5";
    case telemetry::TransmissionGear::Sixth:
      return "D6";
  }

  return "D1";
}

void drawCenteredTextOn(Arduino_GFX *target,
                        const char *text,
                        int16_t center_x,
                        int16_t baseline_y,
                        uint8_t text_size,
                        uint16_t color,
                        uint16_t background) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  target->setFont();
  target->setTextSize(text_size);
  target->getTextBounds(text, 0, baseline_y, &x1, &y1, &width, &height);
  const int16_t cursor_x = center_x - static_cast<int16_t>(width / 2);
  target->setTextColor(color, background);
  target->setCursor(cursor_x, baseline_y);
  target->print(text);
}

void drawCenteredTextInRect(Arduino_GFX *target,
                            const char *text,
                            const Rect &rect,
                            uint8_t text_size,
                            uint16_t color,
                            uint16_t background) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  target->setFont();
  target->setTextSize(text_size);
  target->getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  const int16_t center_x = rect.x + rect.w / 2;
  const int16_t center_y = rect.y + rect.h / 2;
  const int16_t cursor_x = center_x - static_cast<int16_t>(width / 2) - x1;
  const int16_t cursor_y = center_y - static_cast<int16_t>((y1 + height / 2));
  target->setTextColor(color, background);
  target->setCursor(cursor_x, cursor_y);
  target->print(text);
}

void drawCenteredFontTextAt(Arduino_GFX *target,
                            const uint8_t *font,
                            const char *text,
                            int16_t center_x,
                            int16_t center_y,
                            uint16_t color,
                            uint16_t background) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  target->setFont(font);
  target->setTextSize(1);
  target->getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  const int16_t cursor_x = center_x - static_cast<int16_t>(width / 2) - x1;
  const int16_t cursor_y = center_y - static_cast<int16_t>((y1 + height / 2));
  target->setTextColor(color, background);
  target->setCursor(cursor_x, cursor_y);
  target->print(text);
  target->setFont();
}

void drawGrid(telemetry::DriveMode mode) {
  const uint16_t grid = lineColor(mode);
  for (int16_t x = 0; x <= kScreenWidth; x += 28) {
    gfx->drawFastVLine(x, 0, 134, grid);
  }
  for (int16_t y = 0; y <= 134; y += 28) {
    gfx->drawFastHLine(0, y, kScreenWidth, grid);
  }
}

void drawClusterGrid(Arduino_GFX *target, telemetry::DriveMode mode) {
  const uint16_t grid = lineColor(mode);
  for (int16_t local_x = 0; local_x <= kClusterWidth; ++local_x) {
    const int16_t global_x = kClusterX + local_x;
    if ((global_x % 28) == 0) {
      target->drawFastVLine(local_x, 0, kClusterHeight, grid);
    }
  }

  for (int16_t local_y = 0; local_y <= kClusterHeight; ++local_y) {
    const int16_t global_y = kClusterY + local_y;
    if ((global_y % 28) == 0) {
      target->drawFastHLine(0, local_y, kClusterWidth, grid);
    }
  }
}

void drawArcOutlineOn(Arduino_GFX *target,
                      int16_t center_x,
                      int16_t center_y,
                      int16_t radius,
                      float start_degrees,
                      float end_degrees,
                      uint16_t color,
                      float step_degrees = kArcOutlineStepDegrees) {
  float previous_x = 0.0f;
  float previous_y = 0.0f;
  bool has_previous = false;

  for (float degrees = start_degrees; degrees <= end_degrees; degrees += step_degrees) {
    const float radians = degrees * DEG_TO_RAD;
    const float x = center_x + cosf(radians) * radius;
    const float y = center_y + sinf(radians) * radius;
    if (has_previous) {
      target->drawLine(static_cast<int16_t>(previous_x), static_cast<int16_t>(previous_y),
                       static_cast<int16_t>(x), static_cast<int16_t>(y), color);
    }
    previous_x = x;
    previous_y = y;
    has_previous = true;
  }
}

void drawArcBandOn(Arduino_GFX *target,
                   int16_t center_x,
                   int16_t center_y,
                   int16_t outer_radius,
                   int16_t inner_radius,
                   float start_degrees,
                   float end_degrees,
                   uint16_t color) {
  for (int16_t radius = inner_radius; radius <= outer_radius; ++radius) {
    drawArcOutlineOn(target, center_x, center_y, radius, start_degrees, end_degrees, color,
                     kArcBandStepDegrees);
  }
}

void drawGradientArcBandOn(Arduino_GFX *target,
                           int16_t center_x,
                           int16_t center_y,
                           int16_t outer_radius,
                           int16_t inner_radius,
                           float start_degrees,
                           float end_degrees,
                           uint16_t start_color,
                           uint16_t end_color) {
  const float total_span = max(1.0f, end_degrees - start_degrees);
  for (float segment_start = start_degrees; segment_start < end_degrees;
       segment_start += kArcGradientSegmentDegrees) {
    const float segment_end = min(end_degrees, segment_start + kArcGradientSegmentDegrees);
    const float mix = (segment_start - start_degrees) / total_span;
    const uint16_t color = lerpColor565(start_color, end_color, mix);
    drawArcBandOn(target, center_x, center_y, outer_radius, inner_radius, segment_start, segment_end,
                  color);
  }
}

void drawArcCapOn(Arduino_GFX *target,
                  int16_t center_x,
                  int16_t center_y,
                  int16_t radius,
                  float degrees,
                  int16_t cap_radius,
                  uint16_t color) {
  const float radians = degrees * DEG_TO_RAD;
  const int16_t x = static_cast<int16_t>(center_x + cosf(radians) * radius);
  const int16_t y = static_cast<int16_t>(center_y + sinf(radians) * radius);
  target->fillCircle(x, y, cap_radius, color);
}

void drawArcEndpointSet(Arduino_GFX *target,
                        float degrees,
                        uint16_t outline_color,
                        uint16_t band_color) {
  drawArcCapOn(target, kClusterCenterX, kClusterCenterY, kArcOuterRadius - 1, degrees,
               kArcOutlineCapRadius, outline_color);
  drawArcCapOn(target, kClusterCenterX, kClusterCenterY,
               kArcTrackRadius - (kArcTrackThickness / 2), degrees, kArcCapRadius, band_color);
}

void drawTopChrome(const DashboardViewState &view_state,
                   telemetry::DriveMode mode,
                   bool welcome_visible) {
  const uint16_t bg = backgroundColor(mode);
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  const uint16_t accent = accentColor(mode);

  gfx->fillRect(kScreenLabelRegion.x, kScreenLabelRegion.y, kScreenLabelRegion.w,
                kScreenLabelRegion.h, bg);
  gfx->setTextSize(1);
  gfx->setTextColor(accent, bg);
  gfx->setCursor(kScreenLabelRegion.x, 62);
  gfx->print(screenTitle(view_state.active_screen));

  gfx->setTextColor(mutedColor(), bg);
  gfx->setCursor(204, 62);
  gfx->print("speed prioritized for driving glanceability");

  gfx->fillRoundRect(kModePillRegion.x, kModePillRegion.y, kModePillRegion.w, kModePillRegion.h, 12,
                     panel);
  gfx->drawRoundRect(kModePillRegion.x, kModePillRegion.y, kModePillRegion.w, kModePillRegion.h, 12,
                     line);
  drawCenteredTextInRect(gfx, modeLabel(mode), kModePillRegion, 1, textColor(), panel);

  gfx->fillRoundRect(kWelcomeRegion.x, kWelcomeRegion.y, kWelcomeRegion.w, kWelcomeRegion.h, 14,
                     panel);
  gfx->drawRoundRect(kWelcomeRegion.x, kWelcomeRegion.y, kWelcomeRegion.w, kWelcomeRegion.h, 14,
                     line);
  if (welcome_visible) {
    drawCenteredTextInRect(gfx, "Ignition sync complete", kWelcomeRegion, 1, accent, panel);
  } else {
    gfx->fillRoundRect(kWelcomeRegion.x + 2, kWelcomeRegion.y + 2, kWelcomeRegion.w - 4,
                       kWelcomeRegion.h - 4, 12, panel);
  }
}

void drawCluster(const telemetry::DashboardTelemetry &telemetry,
                 telemetry::DriveMode mode,
                 float displayed_rpm) {
  Arduino_GFX *target = cluster_canvas;
  const uint16_t bg = backgroundColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t strong = accentStrongColor(mode);
  const uint16_t progress_start = color565(255, 79, 216);
  const uint16_t progress_mid = color565(157, 108, 255);
  const uint16_t progress_high = color565(108, 150, 255);
  const uint16_t progress_end = color565(57, 228, 255);

  target->fillScreen(bg);
  drawClusterGrid(target, mode);
  drawArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcOuterRadius, kArcOuterRadius - 2,
                180.0f, 360.0f, color565(235, 242, 248));
  drawArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcTrackRadius, kArcInnerRadius,
                180.0f, 360.0f, trackArcColor(mode));
  drawArcEndpointSet(target, 180.0f, color565(235, 242, 248), trackArcColor(mode));
  drawArcEndpointSet(target, 360.0f, color565(235, 242, 248), trackArcColor(mode));

  const float normalized =
      constrain(displayed_rpm / static_cast<float>(kRpmMax), 0.0f, 1.0f);
  const float end_degrees = 180.0f + (180.0f * normalized);
  if (end_degrees > 180.0f) {
    const float first_stop = 180.0f + (end_degrees - 180.0f) * 0.34f;
    const float second_stop = 180.0f + (end_degrees - 180.0f) * 0.68f;
    drawGradientArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcTrackRadius,
                          kArcInnerRadius, 180.0f, first_stop, progress_start, progress_mid);
    drawGradientArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcTrackRadius,
                          kArcInnerRadius, first_stop, second_stop, progress_mid, progress_high);
    drawGradientArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcTrackRadius,
                          kArcInnerRadius, second_stop, end_degrees, progress_high, progress_end);
    drawArcEndpointSet(target, 180.0f, color565(235, 242, 248), progress_start);

    uint16_t end_cap_color = progress_start;
    if (end_degrees > second_stop) {
      end_cap_color = lerpColor565(progress_high, progress_end,
                                   constrain((end_degrees - second_stop) /
                                                 max(1.0f, 360.0f - second_stop),
                                             0.0f, 1.0f));
    } else if (end_degrees > first_stop) {
      end_cap_color = lerpColor565(progress_mid, progress_high,
                                   constrain((end_degrees - first_stop) /
                                                 max(1.0f, second_stop - first_stop),
                                             0.0f, 1.0f));
    } else {
      end_cap_color = lerpColor565(progress_start, progress_mid,
                                   constrain((end_degrees - 180.0f) /
                                                 max(1.0f, first_stop - 180.0f),
                                             0.0f, 1.0f));
    }

    drawArcEndpointSet(target, end_degrees, color565(235, 242, 248), end_cap_color);
  }

  char speed_buffer[8];
  snprintf(speed_buffer, sizeof(speed_buffer), "%d", telemetry.speed_kph);
  drawCenteredFontTextAt(target, u8g2_font_logisoso62_tn, speed_buffer, kClusterCenterX, 130,
                         textColor(), bg);
  drawCenteredTextOn(target, "km/h", kClusterCenterX, 168, 1, strong, bg);

  target->setTextColor(accent, bg);
  target->setTextSize(1);
  target->setCursor(188, 190);
  target->print("orbital driver view");
  target->flush(true);
}

void drawDiamondIcon(int16_t x, int16_t y, uint16_t color) {
  gfx->drawLine(x + 12, y + 2, x + 22, y + 12, color);
  gfx->drawLine(x + 22, y + 12, x + 12, y + 22, color);
  gfx->drawLine(x + 12, y + 22, x + 2, y + 12, color);
  gfx->drawLine(x + 2, y + 12, x + 12, y + 2, color);
}

void drawGearIcon(int16_t x, int16_t y, uint16_t color) {
  gfx->drawFastVLine(x + 12, y + 2, 20, color);
  gfx->drawFastHLine(x + 8, y + 6, 8, color);
  gfx->drawFastHLine(x + 8, y + 12, 8, color);
  gfx->drawFastHLine(x + 8, y + 18, 8, color);
}

void drawRangeIcon(int16_t x, int16_t y, uint16_t color) {
  gfx->drawLine(x + 4, y + 18, x + 18, y + 8, color);
  gfx->drawLine(x + 14, y + 8, x + 18, y + 8, color);
  gfx->drawLine(x + 18, y + 8, x + 18, y + 12, color);
}

void drawFuelIcon(int16_t x, int16_t y, uint16_t color) {
  gfx->drawRect(x + 6, y + 4, 10, 16, color);
  gfx->drawLine(x + 16, y + 8, x + 20, y + 8, color);
  gfx->drawLine(x + 20, y + 8, x + 22, y + 12, color);
  gfx->drawLine(x + 22, y + 12, x + 22, y + 18, color);
}

void drawIconBox(int16_t card_x, uint16_t panel, uint16_t line, void (*drawIcon)(int16_t, int16_t, uint16_t), uint16_t icon_color) {
  gfx->fillRoundRect(card_x + 8, kBottomCardY + 10, 28, 28, 10, color565(18, 24, 30));
  gfx->drawRoundRect(card_x + 8, kBottomCardY + 10, 28, 28, 10, line);
  drawIcon(card_x + 10, kBottomCardY + 12, icon_color);
}

void drawSimpleCard(int16_t card_x,
                    telemetry::DriveMode mode,
                    const char *value,
                    void (*drawIcon)(int16_t, int16_t, uint16_t),
                    uint16_t value_color) {
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  gfx->fillRoundRect(card_x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(card_x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  drawIconBox(card_x, panel, line, drawIcon, accentStrongColor(mode));
  gfx->setTextColor(value_color, panel);
  gfx->setTextSize(2);
  gfx->setCursor(card_x + 46, kBottomCardY + 20);
  gfx->print(value);
}

void drawModeCard(telemetry::DriveMode mode) {
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  const Rect value_region{kBottomCardX0 + 42, kBottomCardY + 6, kBottomCardWidth - 48,
                          kBottomCardHeight - 12};

  gfx->fillRoundRect(kBottomCardX0, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(kBottomCardX0, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  drawIconBox(kBottomCardX0, panel, line, drawDiamondIcon, accentStrongColor(mode));
  drawCenteredTextInRect(gfx, modeLabel(mode), value_region, 1, accentStrongColor(mode), panel);
}

void drawGearCard(telemetry::TransmissionGear gear, telemetry::DriveMode mode) {
  drawSimpleCard(kBottomCardX0 + kBottomCardWidth + kBottomCardGap, mode, gearLabel(gear),
                 drawGearIcon, textColor());
}

void drawMeterBar(int16_t x, int16_t y, int16_t width, uint8_t fill_pct, uint16_t bar_color,
                  uint16_t line) {
  gfx->fillRoundRect(x, y, width, 8, 4, color565(20, 28, 34));
  gfx->drawRoundRect(x, y, width, 8, 4, line);
  const int16_t inner_width = static_cast<int16_t>(((width - 4) * fill_pct) / 100);
  if (inner_width > 0) {
    gfx->fillRoundRect(x + 2, y + 2, inner_width, 4, 2, bar_color);
  }
}

void drawRangeCard(uint16_t range_km, telemetry::DriveMode mode) {
  const int16_t x = kBottomCardX0 + (kBottomCardWidth + kBottomCardGap) * 2;
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  char buffer[16];
  const uint8_t fill_pct = static_cast<uint8_t>(min(100UL, (static_cast<unsigned long>(range_km) * 100UL) / 360UL));

  gfx->fillRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  drawIconBox(x, panel, line, drawRangeIcon, accentStrongColor(mode));
  snprintf(buffer, sizeof(buffer), "%ukm", range_km);
  gfx->setTextColor(textColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(x + 44, kBottomCardY + 20);
  gfx->print(buffer);
  drawMeterBar(x + 44, kBottomCardY + 31, 56, fill_pct, color565(90, 239, 172),
               line);
}

void drawFuelCard(uint8_t fuel_pct, telemetry::DriveMode mode) {
  const int16_t x = kBottomCardX0 + (kBottomCardWidth + kBottomCardGap) * 3;
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  char buffer[16];

  gfx->fillRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  drawIconBox(x, panel, line, drawFuelIcon, accentStrongColor(mode));
  snprintf(buffer, sizeof(buffer), "%u%%", fuel_pct);
  gfx->setTextColor(textColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(x + 44, kBottomCardY + 20);
  gfx->print(buffer);
  drawMeterBar(x + 44, kBottomCardY + 31, 56, fuel_pct, color565(255, 214, 80),
               line);
}

void drawStaticScene(const DashboardViewState &view_state,
                     const telemetry::DashboardTelemetry &telemetry) {
  gfx->fillScreen(backgroundColor(telemetry.drive_mode));
  drawGrid(telemetry.drive_mode);
  drawTopChrome(view_state, telemetry.drive_mode, view_state.welcome_visible);
}

}  // namespace

void DashboardDisplay::begin(Stream &log) {
  Wire.begin(21, 22);

  if (!io_expander.begin()) {
    log.println("[DISPLAY] TCA9554 init failed.");
    return;
  }

  io_expander.pinMode1(kDisplayResetExpanderPin, OUTPUT);
  io_expander.write1(kDisplayResetExpanderPin, HIGH);
  delay(10);
  io_expander.write1(kDisplayResetExpanderPin, LOW);
  delay(10);
  io_expander.write1(kDisplayResetExpanderPin, HIGH);
  delay(200);

  if (!gfx->begin(kDisplaySpiHz)) {
    log.println("[DISPLAY] gfx->begin() failed.");
    return;
  }

  if (!cluster_canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    log.println("[DISPLAY] cluster canvas begin failed.");
    return;
  }

  gfx->setRotation(kDisplayRotation);
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, HIGH);
  gfx->fillScreen(backgroundColor(telemetry::DriveMode::Cruise));

  initialized_ = true;
  log.println("[DISPLAY] ST7796 display ready.");
}

void DashboardDisplay::render(const telemetry::DashboardTelemetry &telemetry,
                              const DashboardViewState &view_state,
                              bool psram_detected,
                              uint32_t psram_size_bytes,
                              uint32_t now_ms) {
  (void)psram_detected;
  (void)psram_size_bytes;

  if (!initialized_) {
    return;
  }

  if (now_ms - last_render_ms_ < kRenderIntervalMs) {
    return;
  }

  const uint32_t delta_ms = last_render_ms_ == 0 ? kRenderIntervalMs : (now_ms - last_render_ms_);
  last_render_ms_ = now_ms;
  const bool mode_changed = !scene_drawn_ || last_mode_ != telemetry.drive_mode;
  const bool scene_changed = !scene_drawn_ || last_screen_ != view_state.active_screen ||
                             mode_changed || last_welcome_visible_ != view_state.welcome_visible;
  const bool speed_changed = !telemetry_cached_ || last_speed_kph_ != telemetry.speed_kph;
  const bool rpm_changed = !telemetry_cached_ || last_rpm_ != telemetry.rpm;
  const bool fuel_changed = !telemetry_cached_ || last_fuel_level_pct_ != telemetry.fuel_level_pct;
  const bool range_changed = !telemetry_cached_ || last_range_km_ != telemetry.estimated_range_km;
  const bool gear_changed = !telemetry_cached_ || last_gear_ != telemetry.gear;

  if (displayed_rpm_ < 0.0f || mode_changed) {
    displayed_rpm_ = static_cast<float>(telemetry.rpm);
  } else {
    const float smoothing = min(1.0f, 0.22f * (static_cast<float>(delta_ms) / 33.0f));
    displayed_rpm_ += (static_cast<float>(telemetry.rpm) - displayed_rpm_) * smoothing;
    if (fabsf(displayed_rpm_ - static_cast<float>(telemetry.rpm)) < 1.0f) {
      displayed_rpm_ = static_cast<float>(telemetry.rpm);
    }
  }
  const int16_t displayed_rpm = static_cast<int16_t>(lroundf(displayed_rpm_));
  const bool arc_animating = !telemetry_cached_ || displayed_rpm != last_arc_rpm_drawn_;

  if (scene_changed) {
    drawStaticScene(view_state, telemetry);
    scene_drawn_ = true;
  }

  if (scene_changed || speed_changed || rpm_changed || arc_animating) {
    drawCluster(telemetry, telemetry.drive_mode, displayed_rpm_);
  }

  if (scene_changed || mode_changed) {
    drawModeCard(telemetry.drive_mode);
  }

  if (scene_changed || gear_changed || mode_changed) {
    drawGearCard(telemetry.gear, telemetry.drive_mode);
  }

  if (scene_changed || range_changed || mode_changed) {
    drawRangeCard(telemetry.estimated_range_km, telemetry.drive_mode);
  }

  if (scene_changed || fuel_changed || mode_changed) {
    drawFuelCard(telemetry.fuel_level_pct, telemetry.drive_mode);
  }

  telemetry_cached_ = true;
  last_arc_rpm_drawn_ = displayed_rpm;
  last_speed_kph_ = telemetry.speed_kph;
  last_rpm_ = telemetry.rpm;
  last_fuel_level_pct_ = telemetry.fuel_level_pct;
  last_range_km_ = telemetry.estimated_range_km;
  last_gear_ = telemetry.gear;
  last_screen_ = view_state.active_screen;
  last_mode_ = telemetry.drive_mode;
  last_welcome_visible_ = view_state.welcome_visible;
}

}  // namespace app