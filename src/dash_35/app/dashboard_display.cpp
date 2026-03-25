#include "dashboard_display.h"

#include <Arduino_GFX_Library.h>
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
constexpr uint32_t kRenderIntervalMs = 100;
constexpr float kPi = 3.14159265f;
constexpr int16_t kScreenWidth = 480;
constexpr int16_t kArcCenterX = 240;
constexpr int16_t kArcCenterY = 222;
constexpr int16_t kArcOuterRadius = 150;
constexpr int16_t kArcTrackRadius = 132;
constexpr int16_t kArcOutlineThickness = 4;
constexpr int16_t kArcTrackThickness = 14;
constexpr int16_t kBottomCardY = 257;
constexpr int16_t kBottomCardWidth = 108;
constexpr int16_t kBottomCardHeight = 48;
constexpr int16_t kBottomCardGap = 8;
constexpr int16_t kBottomCardX0 = 14;
constexpr int16_t kSpeedBoxX = 116;
constexpr int16_t kSpeedBoxY = 118;
constexpr int16_t kSpeedBoxWidth = 248;
constexpr int16_t kSpeedBoxHeight = 102;
constexpr int16_t kRpmMax = 5000;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect kArcRegion{42, 46, 396, 174};
constexpr Rect kScreenLabelRegion{18, 50, 130, 18};
constexpr Rect kModePillRegion{380, 18, 84, 24};
constexpr Rect kWelcomeRegion{140, 16, 200, 28};

TCA9554 io_expander(kIoExpanderAddress);
Arduino_DataBus *display_bus =
    new Arduino_ESP32SPI(kDisplayDcPin, kDisplayCsPin, kDisplaySckPin, kDisplayMosiPin,
                         kDisplayMisoPin);
Arduino_GFX *gfx =
    new Arduino_ST7796(display_bus, GFX_NOT_DEFINED, 0, true /* ips */);

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
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

uint16_t outlineArcColor() {
  return color565(245, 249, 255);
}

uint16_t trackArcColor(telemetry::DriveMode mode) {
  return mode == telemetry::DriveMode::Sport ? color565(84, 56, 56) : color565(70, 92, 112);
}

float degreesToRadians(float degrees) {
  return degrees * kPi / 180.0f;
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

void drawCenteredText(const char *text, int16_t center_x, int16_t baseline_y, uint8_t text_size,
                      uint16_t color, uint16_t background) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  gfx->setTextSize(text_size);
  gfx->getTextBounds(text, 0, baseline_y, &x1, &y1, &width, &height);
  const int16_t cursor_x = center_x - static_cast<int16_t>(width / 2);
  gfx->setTextColor(color, background);
  gfx->setCursor(cursor_x, baseline_y);
  gfx->print(text);
}

void fillArcBand(float start_degrees, float end_degrees, int16_t radius, int16_t thickness,
                 uint16_t color) {
  const int16_t inner_radius = radius - thickness / 2;
  const int16_t outer_radius = radius + thickness / 2;

  for (float angle = start_degrees; angle < end_degrees; angle += 3.0f) {
    const float next_angle = min(end_degrees, angle + 3.0f);
    const float angle_rad = degreesToRadians(angle);
    const float next_angle_rad = degreesToRadians(next_angle);

    const int16_t x1_outer = kArcCenterX + cosf(angle_rad) * outer_radius;
    const int16_t y1_outer = kArcCenterY + sinf(angle_rad) * outer_radius;
    const int16_t x2_outer = kArcCenterX + cosf(next_angle_rad) * outer_radius;
    const int16_t y2_outer = kArcCenterY + sinf(next_angle_rad) * outer_radius;
    const int16_t x1_inner = kArcCenterX + cosf(angle_rad) * inner_radius;
    const int16_t y1_inner = kArcCenterY + sinf(angle_rad) * inner_radius;
    const int16_t x2_inner = kArcCenterX + cosf(next_angle_rad) * inner_radius;
    const int16_t y2_inner = kArcCenterY + sinf(next_angle_rad) * inner_radius;

    gfx->fillTriangle(x1_inner, y1_inner, x1_outer, y1_outer, x2_outer, y2_outer, color);
    gfx->fillTriangle(x1_inner, y1_inner, x2_inner, y2_inner, x2_outer, y2_outer, color);
  }
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
  drawCenteredText(modeLabel(mode), kModePillRegion.x + kModePillRegion.w / 2, 34, 1, textColor(),
                   panel);

  gfx->fillRoundRect(kWelcomeRegion.x, kWelcomeRegion.y, kWelcomeRegion.w, kWelcomeRegion.h, 14,
                     panel);
  gfx->drawRoundRect(kWelcomeRegion.x, kWelcomeRegion.y, kWelcomeRegion.w, kWelcomeRegion.h, 14,
                     line);
  if (welcome_visible) {
    drawCenteredText("Ignition sync complete", kWelcomeRegion.x + kWelcomeRegion.w / 2, 35, 1,
                     accent, panel);
  } else {
    gfx->fillRoundRect(kWelcomeRegion.x + 2, kWelcomeRegion.y + 2, kWelcomeRegion.w - 4,
                       kWelcomeRegion.h - 4, 12, panel);
  }
}

void drawArcBase(telemetry::DriveMode mode) {
  fillArcBand(180.0f, 360.0f, kArcOuterRadius, kArcOutlineThickness, outlineArcColor());
  fillArcBand(180.0f, 360.0f, kArcTrackRadius, kArcTrackThickness, trackArcColor(mode));
}

void drawArcProgress(int16_t rpm) {
  const float normalized = constrain(static_cast<float>(rpm) / static_cast<float>(kRpmMax), 0.0f,
                                     1.0f);
  const float end_degrees = 180.0f + (180.0f * normalized);

  if (end_degrees <= 180.0f) {
    return;
  }

  const float first_stop = 180.0f + (end_degrees - 180.0f) * 0.34f;
  const float second_stop = 180.0f + (end_degrees - 180.0f) * 0.68f;
  fillArcBand(180.0f, first_stop, kArcTrackRadius, kArcTrackThickness, color565(255, 79, 216));
  fillArcBand(first_stop, second_stop, kArcTrackRadius, kArcTrackThickness, color565(157, 108, 255));
  fillArcBand(second_stop, end_degrees, kArcTrackRadius, kArcTrackThickness, color565(57, 228, 255));
}

void drawArcAndSpeed(const telemetry::DashboardTelemetry &telemetry, telemetry::DriveMode mode) {
  const uint16_t bg = backgroundColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t strong = accentStrongColor(mode);

  gfx->fillRect(kArcRegion.x, kArcRegion.y, kArcRegion.w, kArcRegion.h, bg);
  drawArcBase(mode);
  drawArcProgress(telemetry.rpm);
  gfx->fillRoundRect(kSpeedBoxX, kSpeedBoxY, kSpeedBoxWidth, kSpeedBoxHeight, 36, bg);

  char speed_buffer[8];
  snprintf(speed_buffer, sizeof(speed_buffer), "%d", telemetry.speed_kph);
  drawCenteredText(speed_buffer, kArcCenterX, 180, 6, textColor(), bg);
  drawCenteredText("km/h", kArcCenterX, 214, 2, strong, bg);

  gfx->setTextColor(accent, bg);
  gfx->setTextSize(1);
  gfx->setCursor(187, 230);
  gfx->print("orbital driver view");
}

void drawModeCard(telemetry::DriveMode mode) {
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);

  gfx->fillRoundRect(kBottomCardX0, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(kBottomCardX0, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  gfx->setTextColor(mutedColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(kBottomCardX0 + 10, kBottomCardY + 13);
  gfx->print("Mode");
  gfx->setTextColor(accentStrongColor(mode), panel);
  gfx->setTextSize(2);
  gfx->setCursor(kBottomCardX0 + 10, kBottomCardY + 30);
  gfx->print(modeLabel(mode));
}

void drawGearCard(telemetry::TransmissionGear gear, telemetry::DriveMode mode) {
  const int16_t x = kBottomCardX0 + kBottomCardWidth + kBottomCardGap;
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);

  gfx->fillRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  gfx->setTextColor(mutedColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, kBottomCardY + 13);
  gfx->print("Gear");
  gfx->setTextColor(textColor(), panel);
  gfx->setTextSize(2);
  gfx->setCursor(x + 10, kBottomCardY + 30);
  gfx->print(gearLabel(gear));
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
  gfx->setTextColor(mutedColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, kBottomCardY + 13);
  gfx->print("Range");
  snprintf(buffer, sizeof(buffer), "%ukm", range_km);
  gfx->setTextColor(textColor(), panel);
  gfx->setTextSize(2);
  gfx->setCursor(x + 10, kBottomCardY + 28);
  gfx->print(buffer);
  drawMeterBar(x + 10, kBottomCardY + 37, kBottomCardWidth - 20, fill_pct, color565(90, 239, 172),
               line);
}

void drawFuelCard(uint8_t fuel_pct, telemetry::DriveMode mode) {
  const int16_t x = kBottomCardX0 + (kBottomCardWidth + kBottomCardGap) * 3;
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  char buffer[16];

  gfx->fillRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, panel);
  gfx->drawRoundRect(x, kBottomCardY, kBottomCardWidth, kBottomCardHeight, 14, line);
  gfx->setTextColor(mutedColor(), panel);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, kBottomCardY + 13);
  gfx->print("Fuel");
  snprintf(buffer, sizeof(buffer), "%u%%", fuel_pct);
  gfx->setTextColor(textColor(), panel);
  gfx->setTextSize(2);
  gfx->setCursor(x + 10, kBottomCardY + 28);
  gfx->print(buffer);
  drawMeterBar(x + 10, kBottomCardY + 37, kBottomCardWidth - 20, fuel_pct, color565(255, 214, 80),
               line);
}

void drawBottomStrip(const telemetry::DashboardTelemetry &telemetry) {
  drawModeCard(telemetry.drive_mode);
  drawGearCard(telemetry.gear, telemetry.drive_mode);
  drawRangeCard(telemetry.estimated_range_km, telemetry.drive_mode);
  drawFuelCard(telemetry.fuel_level_pct, telemetry.drive_mode);
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

  last_render_ms_ = now_ms;
  const bool scene_changed = !scene_drawn_ || last_screen_ != view_state.active_screen ||
                             last_mode_ != telemetry.drive_mode ||
                             last_welcome_visible_ != view_state.welcome_visible;

  if (scene_changed) {
    drawStaticScene(view_state, telemetry);
    scene_drawn_ = true;
    last_screen_ = view_state.active_screen;
    last_mode_ = telemetry.drive_mode;
    last_welcome_visible_ = view_state.welcome_visible;
  }

  drawArcAndSpeed(telemetry, telemetry.drive_mode);
  drawBottomStrip(telemetry);
}

}  // namespace app