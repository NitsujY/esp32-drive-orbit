#include "dashboard_display.h"

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <TCA9554.h>
#include <Wire.h>

#include <math.h>
#include <stdio.h>

#include "cyber_theme.h"
#include "vehicle_profiles/vehicle_profile.h"

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
constexpr Rect kObdStatusRegion{152, 50, 208, 18};
constexpr Rect kModePillRegion{380, 18, 84, 24};
constexpr Rect kWelcomeRegion{140, 16, 200, 28};
constexpr Rect kClusterSubtitleRegion{138, 174, 204, 14};

TCA9554 io_expander(kIoExpanderAddress);
Arduino_DataBus *display_bus =
    new Arduino_ESP32SPI(kDisplayDcPin, kDisplayCsPin, kDisplaySckPin, kDisplayMosiPin,
                         kDisplayMisoPin);
Arduino_GFX *gfx =
    new Arduino_ST7796(display_bus, GFX_NOT_DEFINED, 0, true /* ips */);
Arduino_Canvas *cluster_canvas =
  new Arduino_Canvas(kClusterWidth, kClusterHeight, gfx, kClusterX, kClusterY, 0);

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return theme::rgb565(r, g, b);
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
  return theme::bg(static_cast<uint8_t>(mode));
}

uint16_t panelColor(telemetry::DriveMode mode) {
  return theme::panel(static_cast<uint8_t>(mode));
}

uint16_t lineColor(telemetry::DriveMode mode) {
  return theme::line(static_cast<uint8_t>(mode));
}

uint16_t accentColor(telemetry::DriveMode mode) {
  return theme::accent(static_cast<uint8_t>(mode));
}

uint16_t accentStrongColor(telemetry::DriveMode mode) {
  return theme::accentStrong(static_cast<uint8_t>(mode));
}

uint16_t mutedColor() {
  return theme::muted();
}

uint16_t textColor() {
  return theme::text();
}

uint16_t trackArcColor(telemetry::DriveMode mode) {
  return theme::track(static_cast<uint8_t>(mode));
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
    case telemetry::TransmissionGear::Park:
      return "P";
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

  return "P";
}

const char *obdStatusDetail(ObdConnectionState state) {
  switch (state) {
    case ObdConnectionState::Disconnected:
      return "Waiting for connection";
    case ObdConnectionState::Connecting:
      return "Scanning";
    case ObdConnectionState::Live:
      return "Connected";
  }

  return "Waiting for connection";
}

uint16_t obdStatusColor(ObdConnectionState state, telemetry::DriveMode mode) {
  switch (state) {
    case ObdConnectionState::Disconnected:
      return mutedColor();
    case ObdConnectionState::Connecting:
      return mode == telemetry::DriveMode::Sport ? color565(255, 190, 135)
                                                 : color565(255, 214, 128);
    case ObdConnectionState::Live:
      return accentColor(mode);
  }

  return mutedColor();
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
  (void)mode;
  // Clean background — no pattern.
}

void drawClusterGrid(Arduino_GFX *target, telemetry::DriveMode mode) {
  (void)target;
  (void)mode;
  // Clean background — no pattern.
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
  const uint16_t accent = accentColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));

  // Clean top bar: vehicle name left, status right, thin accent line below.
  gfx->fillRect(0, 0, kScreenWidth, 52, bg);

  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(accent, bg);
  gfx->setCursor(16, 14);
  gfx->print(vehicle_profiles::activeProfile().display_name);

  // Mode label next to vehicle name.
  gfx->setTextColor(dim, bg);
  gfx->setCursor(16, 30);
  gfx->print(modeLabel(mode));

  // OBD status on the right.
  const char *status_text = nullptr;
  uint16_t status_color = mutedColor();
  if (view_state.obd_connection_state != ObdConnectionState::Live) {
    status_text = obdStatusDetail(view_state.obd_connection_state);
    status_color = obdStatusColor(view_state.obd_connection_state, mode);
  } else if (welcome_visible) {
    status_text = "Ignition sync";
    status_color = accent;
  } else {
    status_text = "Connected";
    status_color = accent;
  }
  gfx->setTextColor(status_color, bg);
  gfx->setCursor(360, 14);
  gfx->print(status_text);

  // Thin neon accent line.
  gfx->drawFastHLine(12, 48, kScreenWidth - 24, dim);
}

void drawCluster(const telemetry::DashboardTelemetry &telemetry,
                 const DashboardViewState &view_state,
                 telemetry::DriveMode mode,
                 float displayed_rpm) {
  Arduino_GFX *target = cluster_canvas;
  const uint16_t bg = backgroundColor(mode);
  const uint16_t strong = accentStrongColor(mode);
  const uint16_t progress_start = theme::arcStart();
  const uint16_t progress_mid = theme::arcMid();
  const uint16_t progress_high = theme::arcHigh();
  const uint16_t progress_end = theme::arcEnd();

  target->fillScreen(bg);
  drawClusterGrid(target, mode);
  drawArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcOuterRadius, kArcOuterRadius - 2,
                180.0f, 360.0f, theme::accentDim(static_cast<uint8_t>(mode)));
  drawArcBandOn(target, kClusterCenterX, kClusterCenterY, kArcTrackRadius, kArcInnerRadius,
                180.0f, 360.0f, trackArcColor(mode));
  drawArcEndpointSet(target, 180.0f, theme::accentDim(static_cast<uint8_t>(mode)), trackArcColor(mode));
  drawArcEndpointSet(target, 360.0f, theme::accentDim(static_cast<uint8_t>(mode)), trackArcColor(mode));

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
    drawArcEndpointSet(target, 180.0f, theme::accentDim(static_cast<uint8_t>(mode)), progress_start);

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

    drawArcEndpointSet(target, end_degrees, theme::accentDim(static_cast<uint8_t>(mode)), end_cap_color);
  }

  // Small RPM number at the right end of the arc
  char rpm_buffer[8];
  snprintf(rpm_buffer, sizeof(rpm_buffer), "%d", static_cast<int>(displayed_rpm));
  target->setFont();
  target->setTextSize(1);
  target->setTextColor(mutedColor(), bg);
  target->setCursor(kClusterCenterX + kArcOuterRadius - 28, kClusterCenterY - 4);
  target->print(rpm_buffer);

  char speed_buffer[8];
  snprintf(speed_buffer, sizeof(speed_buffer), "%d", telemetry.speed_kph);
  drawCenteredFontTextAt(target, u8g2_font_logisoso62_tn, speed_buffer, kClusterCenterX, 130,
                         textColor(), bg);
  drawCenteredTextOn(target, "km/h", kClusterCenterX, 168, 1, strong, bg);

  char subtitle[48];
  snprintf(subtitle, sizeof(subtitle), "trip %.1f km", view_state.trip.trip_distance_km);
  drawCenteredTextInRect(target, subtitle, kClusterSubtitleRegion, 1, mutedColor(), bg);
  target->flush(true);
}

void drawDiamondIcon(int16_t, int16_t, uint16_t) {}
void drawGearIcon(int16_t, int16_t, uint16_t) {}
void drawRangeIcon(int16_t, int16_t, uint16_t) {}
void drawFuelIcon(int16_t, int16_t, uint16_t) {}
void drawIconBox(int16_t, uint16_t, uint16_t, void (*)(int16_t, int16_t, uint16_t), uint16_t) {}

void drawSimpleCard(int16_t, telemetry::DriveMode, const char *, void (*)(int16_t, int16_t, uint16_t), uint16_t) {}

void drawModeCard(telemetry::DriveMode mode) {
  // Floating text: mode label at bottom-left.
  const uint16_t bg = backgroundColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));

  gfx->fillRect(0, kBottomCardY - 6, kScreenWidth, kBottomCardHeight + 12, bg);

  // Thin separator line.
  gfx->drawFastHLine(12, kBottomCardY - 4, kScreenWidth - 24, dim);

  // Mode.
  gfx->setFont();
  gfx->setTextSize(2);
  gfx->setTextColor(accent, bg);
  gfx->setCursor(16, kBottomCardY + 12);
  gfx->print(modeLabel(mode));
}

void drawGearCard(telemetry::TransmissionGear gear, telemetry::DriveMode mode) {
  const uint16_t bg = backgroundColor(mode);
  gfx->setFont();
  gfx->setTextSize(2);
  gfx->setTextColor(textColor(), bg);
  gfx->setCursor(130, kBottomCardY + 12);
  gfx->print(gearLabel(gear));
  gfx->print("  ");
}

void drawRangeCard(uint16_t range_km, telemetry::DriveMode mode) {
  const uint16_t bg = backgroundColor(mode);
  char buf[16];
  snprintf(buf, sizeof(buf), "%ukm", range_km);
  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(mutedColor(), bg);
  gfx->setCursor(280, kBottomCardY + 16);
  gfx->print(buf);
  gfx->print("   ");
}

void drawFuelCard(uint8_t fuel_pct, telemetry::DriveMode mode) {
  const uint16_t bg = backgroundColor(mode);
  const bool low_fuel = fuel_pct < 15 && fuel_pct > 0;
  const uint16_t color = low_fuel ? theme::warning() : accentColor(mode);
  char buf[16];
  snprintf(buf, sizeof(buf), "%u%%", fuel_pct);
  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(color, bg);
  gfx->setCursor(380, kBottomCardY + 16);
  gfx->print(buf);
  gfx->print("   ");
  if (low_fuel) {
    gfx->setCursor(420, kBottomCardY + 16);
    gfx->setTextColor(theme::warning(), bg);
    gfx->print("LOW");
  }
}

void drawStaticScene(const DashboardViewState &view_state,
                     const telemetry::DashboardTelemetry &telemetry) {
  gfx->fillScreen(backgroundColor(telemetry.drive_mode));
  drawTopChrome(view_state, telemetry.drive_mode, view_state.welcome_visible);
}

void drawDetailView(const telemetry::DashboardTelemetry &telemetry,
                    const DashboardViewState &view_state,
                    telemetry::DriveMode mode) {
  const uint16_t bg = backgroundColor(mode);
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t text = textColor();
  const uint16_t muted = mutedColor();

  gfx->fillScreen(bg);

  gfx->setTextSize(1);
  gfx->setTextColor(accent, bg);
  gfx->setCursor(16, 14);
  gfx->print(vehicle_profiles::activeProfile().display_name);

  gfx->setTextColor(muted, bg);
  gfx->setCursor(16, 30);
  gfx->print("Vehicle Detail");

  // Thin accent separator.
  gfx->drawFastHLine(12, 46, kScreenWidth - 24, line);

  constexpr int16_t kLabelX = 24;
  constexpr int16_t kValueX = 280;
  constexpr int16_t kRowHeight = 32;
  int16_t y = 60;

  auto drawRow = [&](const char *label, const char *value) {
    gfx->fillRoundRect(16, y, 448, kRowHeight - 4, 8, panel);
    gfx->drawRoundRect(16, y, 448, kRowHeight - 4, 8, line);
    gfx->setTextColor(muted, panel);
    gfx->setCursor(kLabelX, y + 9);
    gfx->print(label);
    gfx->setTextColor(text, panel);
    gfx->setCursor(kValueX, y + 9);
    gfx->print(value);
    y += kRowHeight;
  };

  char buf[32];
  snprintf(buf, sizeof(buf), "%d C", telemetry.coolant_temp_c);
  drawRow("Coolant Temp", buf);
  snprintf(buf, sizeof(buf), "%.1f V", telemetry.battery_mv / 1000.0f);
  drawRow("Battery", buf);
  snprintf(buf, sizeof(buf), "%u%%", telemetry.fuel_level_pct);
  drawRow("Fuel Level", buf);
  snprintf(buf, sizeof(buf), "%u km", telemetry.estimated_range_km);
  drawRow("Est. Range", buf);
  snprintf(buf, sizeof(buf), "%.1f km", view_state.trip.trip_distance_km);
  drawRow("Trip Distance", buf);
  snprintf(buf, sizeof(buf), "%s", gearLabel(telemetry.gear));
  drawRow("Gear", buf);

  gfx->setTextColor(muted, bg);
  gfx->setCursor(150, 300);
  gfx->print("press BOOT to return");
}

constexpr uint8_t kButtonBootPin = 0;

bool readButtonDown() {
  return digitalRead(kButtonBootPin) == LOW;
}

}  // namespace

void DashboardDisplay::begin(Stream &log) {
  log_output_ = &log;
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

  // BOOT button (GPIO 0) for detail view toggle
  pinMode(kButtonBootPin, INPUT_PULLUP);
  log.println("[DISPLAY] BOOT button ready.");
}

bool DashboardDisplay::pollButton(uint32_t now_ms) {
  if (now_ms - last_button_ms_ < 80) {
    return button_was_down_;
  }
  last_button_ms_ = now_ms;
  return readButtonDown();
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

  const bool input_down = pollButton(now_ms);
  if (input_down && !button_was_down_ && now_ms - last_toggle_ms_ > 500) {
    detail_view_ = !detail_view_;
    scene_drawn_ = false;
    telemetry_cached_ = false;
    last_toggle_ms_ = now_ms;
  }
  if (!input_down) {
    button_was_down_ = false;
  } else {
    button_was_down_ = true;
  }

  const uint32_t delta_ms = last_render_ms_ == 0 ? kRenderIntervalMs : (now_ms - last_render_ms_);
  last_render_ms_ = now_ms;

  if (detail_view_) {
    if (!telemetry_cached_) {
      drawDetailView(telemetry, view_state, telemetry.drive_mode);
      telemetry_cached_ = true;
    }
    last_speed_kph_ = telemetry.speed_kph;
    last_rpm_ = telemetry.rpm;
    last_fuel_level_pct_ = telemetry.fuel_level_pct;
    last_range_km_ = telemetry.estimated_range_km;
    last_gear_ = telemetry.gear;
    last_obd_connection_state_ = view_state.obd_connection_state;
    last_mode_ = telemetry.drive_mode;
    last_welcome_visible_ = view_state.welcome_visible;
    return;
  }

  if (last_detail_view_) {
    scene_drawn_ = false;
    telemetry_cached_ = false;
    last_detail_view_ = false;
  }

  const bool mode_changed = !scene_drawn_ || last_mode_ != telemetry.drive_mode;
  // Only trigger full scene redraw on Live transition, not Disconnected↔Connecting flicker
  const bool obd_became_live = !scene_drawn_ ||
      (view_state.obd_connection_state == ObdConnectionState::Live &&
       last_obd_connection_state_ != ObdConnectionState::Live);
  const bool obd_lost_live = last_obd_connection_state_ == ObdConnectionState::Live &&
       view_state.obd_connection_state != ObdConnectionState::Live;
  const bool scene_changed = !scene_drawn_ || last_screen_ != view_state.active_screen ||
                             mode_changed || last_welcome_visible_ != view_state.welcome_visible ||
                             obd_became_live || obd_lost_live;
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
    drawCluster(telemetry, view_state, telemetry.drive_mode, displayed_rpm_);
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
  last_obd_connection_state_ = view_state.obd_connection_state;
  last_mode_ = telemetry.drive_mode;
  last_welcome_visible_ = view_state.welcome_visible;
}

}  // namespace app