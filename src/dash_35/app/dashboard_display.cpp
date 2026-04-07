#include "dashboard_display.h"

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <TCA9554.h>
#include <Wire.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cyber_theme.h"
#include "dashboard_scene.h"
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
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kRpmMax = 5000;
constexpr uint8_t kTachSegmentCount = 20;
constexpr int16_t kTachX = 42;
constexpr int16_t kTachY = 202;
constexpr int16_t kTachWidth = 396;
constexpr int16_t kTachHeight = 54;
constexpr int16_t kSpeedCenterX = 240;
constexpr int16_t kSpeedCenterY = 122;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect kClusterPanel{10, 12, 460, 296};
constexpr Rect kSpeedPanel{74, 56, 332, 138};
constexpr Rect kReminderRect{134, 34, 212, 18};
constexpr Rect kDriveRect{28, 274, 100, 20};
constexpr Rect kEnergyRect{272, 274, 180, 20};

TCA9554 io_expander(kIoExpanderAddress);
Arduino_DataBus *display_bus =
    new Arduino_ESP32SPI(kDisplayDcPin, kDisplayCsPin, kDisplaySckPin, kDisplayMosiPin,
                         kDisplayMisoPin);
Arduino_GFX *panel_gfx =
    new Arduino_ST7796(display_bus, GFX_NOT_DEFINED, 0, true /* ips */);
Arduino_Canvas *framebuffer_gfx = nullptr;
Arduino_GFX *gfx = panel_gfx;

void flushDisplay() {
  if (framebuffer_gfx != nullptr && gfx == framebuffer_gfx) {
    framebuffer_gfx->flush();
  }
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

uint16_t lineColor(telemetry::DriveMode mode) {
  return theme::line(static_cast<uint8_t>(mode));
}

uint16_t mutedColor() {
  return theme::muted();
}

uint16_t textColor() {
  return theme::text();
}

void buildClockLabel(char *time_buffer,
                     size_t time_buffer_size,
                     bool *synced_out,
                     int32_t *clock_key_out,
                     int8_t *hour_out = nullptr) {
  struct tm local_time_info {};
  const bool synced = getLocalTime(&local_time_info, 0);

  if (synced) {
    buildDashboardClockLabel(local_time_info.tm_hour, local_time_info.tm_min, true, time_buffer,
                             time_buffer_size);
    if (clock_key_out != nullptr) {
      *clock_key_out = (local_time_info.tm_yday * 1440) +
                       (local_time_info.tm_hour * 60) + local_time_info.tm_min;
    }
    if (hour_out != nullptr) {
      *hour_out = static_cast<int8_t>(local_time_info.tm_hour);
    }
  } else {
    snprintf(time_buffer, time_buffer_size, "--:--");
    if (clock_key_out != nullptr) {
      *clock_key_out = -1;
    }
    if (hour_out != nullptr) {
      *hour_out = -1;
    }
  }

  if (synced_out != nullptr) {
    *synced_out = synced;
  }
}

const char *weatherSummaryText(uint8_t weather_code) {
  if (weather_code == telemetry::kWeatherCodeUnknown) {
    return "CLEAR";
  }

  return weather_code < 50 ? "FAIR" : "RAIN";
}

void buildRollingStatusText(char *buffer,
                            size_t buffer_size,
                            const telemetry::DashboardTelemetry &telemetry,
                            const DashboardViewState &view_state) {
  if (view_state.telemetry_data_mode == TelemetryDataMode::Fallback) {
    switch (view_state.obd_connection_state) {
      case ObdConnectionState::Live:
        snprintf(buffer, buffer_size, "OBD STALE");
        return;
      case ObdConnectionState::Connecting:
        snprintf(buffer, buffer_size, "OBD SCAN");
        return;
      case ObdConnectionState::Disconnected:
        snprintf(buffer, buffer_size, "NO OBD DATA");
        return;
    }
  }

  switch ((telemetry.uptime_ms / 2400U) % 4U) {
    case 0:
      snprintf(buffer, buffer_size, "COOLANT %dC", telemetry.coolant_temp_c);
      break;
    case 1:
      snprintf(buffer, buffer_size, "BATTERY %.1fV", telemetry.battery_mv / 1000.0f);
      break;
    case 2:
      if (telemetry.nearest_camera_m != telemetry::kNearestCameraUnknown && telemetry.nearest_camera_m <= 1000) {
        snprintf(buffer, buffer_size, "CAM %um", telemetry.nearest_camera_m);
      } else {
        snprintf(buffer, buffer_size, "TRIP %.1fKM", view_state.trip.trip_distance_km);
      }
      break;
    default:
      snprintf(buffer, buffer_size, "%s", telemetry.wifi_connected ? "WIFI LINKED" : "WIFI OFF");
      break;
  }
}

bool shouldShowHeadlightReminder(const telemetry::DashboardTelemetry &telemetry,
                                 bool time_synced,
                                 int8_t local_hour) {
  if (telemetry.headlights_on || telemetry.speed_kph <= 0 || !time_synced || local_hour < 0) {
    return false;
  }

  return local_hour >= 18 || local_hour < 6;
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

void drawTopChrome(const DashboardSnapshot &snapshot,
                   const telemetry::DashboardTelemetry &telemetry,
                   const DashboardViewState &view_state) {
  const uint16_t panel = theme::rgb565(7, 18, 30);
  const uint16_t muted = mutedColor();
  const uint16_t strong = theme::rgb565(226, 249, 255);
  const uint16_t weatherAccent = strong;
  char rolling_text[24];
  buildRollingStatusText(rolling_text, sizeof(rolling_text), telemetry, view_state);
  char temp_text[8];
  if (telemetry.weather_temp_c == telemetry::kWeatherTempUnknown) {
    snprintf(temp_text, sizeof(temp_text), "--C");
  } else {
    snprintf(temp_text, sizeof(temp_text), "%dC", telemetry.weather_temp_c);
  }

  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(strong, panel);
  gfx->setCursor(22, 18);
  gfx->print(weatherSummaryText(telemetry.weather_code));
  gfx->setTextColor(muted, panel);
  gfx->setCursor(72, 18);
  gfx->print("/");
  gfx->setTextColor(weatherAccent, panel);
  gfx->setCursor(84, 18);
  gfx->print(temp_text);

  drawCenteredTextInRect(gfx, rolling_text, Rect{168, 11, 144, 18}, 1, strong, panel);
  gfx->drawFastHLine(154, 19, 10, muted);
  gfx->drawFastHLine(316, 19, 10, muted);
  gfx->setTextColor(strong, panel);
  gfx->setCursor(402, 18);
  gfx->print(snapshot.clock_text);
}

void drawSpeedBackdrop(Arduino_GFX *target, telemetry::DriveMode mode) {
  const uint16_t soft = theme::rgb565(22, 86, 110);
  target->fillTriangle(kSpeedPanel.x - 38, kSpeedPanel.y + 28,
                       kSpeedPanel.x - 8, kSpeedPanel.y + 28,
                       kSpeedPanel.x - 20, kSpeedPanel.y + 58, soft);
  target->fillTriangle(kSpeedPanel.x - 38, kSpeedPanel.y + 28,
                       kSpeedPanel.x - 20, kSpeedPanel.y + 58,
                       kSpeedPanel.x - 52, kSpeedPanel.y + 58, soft);
  target->fillTriangle(kSpeedPanel.x + kSpeedPanel.w + 8, kSpeedPanel.y + 28,
                       kSpeedPanel.x + kSpeedPanel.w + 38, kSpeedPanel.y + 28,
                       kSpeedPanel.x + kSpeedPanel.w + 52, kSpeedPanel.y + 58, soft);
  target->fillTriangle(kSpeedPanel.x + kSpeedPanel.w + 8, kSpeedPanel.y + 28,
                       kSpeedPanel.x + kSpeedPanel.w + 52, kSpeedPanel.y + 58,
                       kSpeedPanel.x + kSpeedPanel.w + 20, kSpeedPanel.y + 58, soft);
}

void drawTachRibbon(Arduino_GFX *target, telemetry::DriveMode mode, float displayed_rpm) {
  const uint16_t line = theme::rgb565(84, 226, 244);
  const uint16_t accent = theme::rgb565(116, 242, 250);
  const uint16_t accent_alt = theme::rgb565(34, 196, 226);
  const uint16_t strong = theme::rgb565(210, 252, 255);
  const uint16_t track = theme::rgb565(25, 60, 80);
  const uint16_t redline = theme::rgb565(255, 154, 94);
  const uint8_t active_segments = static_cast<uint8_t>(roundf(constrain(
      displayed_rpm / static_cast<float>(kRpmMax), 0.0f, 1.0f) * kTachSegmentCount));

  constexpr int16_t kSegmentInsetX = 0;
  constexpr int16_t kSegmentGap = 4;
  const int16_t baseline_y = kTachY + kTachHeight - 12;
  const int16_t segment_width =
      (kTachWidth - (kSegmentInsetX * 2) - ((kTachSegmentCount - 1) * kSegmentGap)) / kTachSegmentCount;

  target->drawFastHLine(kTachX - 8, baseline_y + 6, kTachWidth + 14, line);
  target->drawFastVLine(kTachX - 2, baseline_y + 2, 8, strong);
  target->drawFastVLine(kTachX + 150, baseline_y + 3, 6, strong);
  target->drawFastVLine(kTachX + 284, baseline_y + 3, 6, strong);
  target->drawFastHLine(kTachX + kTachWidth - 8, baseline_y + 4, 26, redline);

  for (uint8_t index = 0; index < kTachSegmentCount; ++index) {
    const int16_t segment_height = 18 + ((index * 26) / max<uint8_t>(1, kTachSegmentCount - 1));
    const int16_t x = kTachX + kSegmentInsetX + (index * (segment_width + kSegmentGap));
    const int16_t y = baseline_y - segment_height;
    uint16_t color = track;

    if (index < active_segments) {
      const float mix = static_cast<float>(index) /
                        static_cast<float>(max<int>(1, kTachSegmentCount - 1));
      color = lerpColor565(accent, accent_alt, mix * 0.72f);
    }

    target->fillTriangle(x, y, x + segment_width, y, x + 4, baseline_y, color);
    target->fillTriangle(x + segment_width, y, x + segment_width + 4, baseline_y,
                         x + 4, baseline_y, color);
  }

  drawCenteredTextInRect(target, "RPM", Rect{kTachX + 120, baseline_y + 10, 160, 16}, 1, strong,
                         backgroundColor(mode));
}

void drawReminderZone(Arduino_GFX *target, telemetry::DriveMode mode) {
  const uint16_t fill = theme::rgb565(39, 20, 12);
  const uint16_t line = theme::rgb565(255, 164, 94);
  const uint16_t text = theme::rgb565(255, 224, 194);
  const uint16_t accent = theme::rgb565(255, 196, 132);

  target->fillRoundRect(kReminderRect.x, kReminderRect.y, kReminderRect.w, kReminderRect.h, 8, fill);
  target->drawRoundRect(kReminderRect.x, kReminderRect.y, kReminderRect.w, kReminderRect.h, 8, line);
  target->drawFastHLine(kReminderRect.x - 10, kReminderRect.y + (kReminderRect.h / 2), 8, accent);
  target->drawFastHLine(kReminderRect.x + kReminderRect.w + 2,
                        kReminderRect.y + (kReminderRect.h / 2), 8, accent);
  drawCenteredTextInRect(target, "LIGHTS OFF AFTER SUNSET", kReminderRect, 1, text, fill);
}

const char *scoreBandLabel(uint8_t score) {
  if (score >= 88) {
    return "EXCELLENT";
  }
  if (score >= 72) {
    return "STEADY";
  }
  if (score >= 56) {
    return "WATCH";
  }
  return "PUSHY";
}

uint16_t scoreTint(uint8_t score) {
  if (score >= 84) {
    return theme::rgb565(116, 242, 250);
  }
  if (score >= 68) {
    return theme::rgb565(255, 204, 102);
  }
  return theme::rgb565(255, 122, 96);
}

void buildCoachingLines(const char *message,
                        char *line_one,
                        size_t line_one_size,
                        char *line_two,
                        size_t line_two_size) {
  line_one[0] = '\0';
  line_two[0] = '\0';

  if (message == nullptr || message[0] == '\0') {
    return;
  }

  const size_t message_length = strlen(message);
  if (message_length <= 46U) {
    snprintf(line_one, line_one_size, "%.*s", static_cast<int>(line_one_size - 1), message);
    return;
  }

  size_t split_index = 46U;
  while (split_index > 24U && message[split_index] != ' ') {
    --split_index;
  }
  if (split_index <= 24U) {
    split_index = 46U;
  }

  snprintf(line_one, line_one_size, "%.*s", static_cast<int>(split_index), message);
  while (message[split_index] == ' ') {
    ++split_index;
  }
  snprintf(line_two, line_two_size, "%s", message + split_index);
}

void drawScoreCard(Arduino_GFX *target,
                   const Rect &rect,
                   telemetry::DriveMode mode,
                   const char *label,
                   uint8_t score) {
  const uint16_t panel = theme::rgb565(10, 24, 36);
  const uint16_t line = lineColor(mode);
  const uint16_t muted = mutedColor();
  const uint16_t strong = textColor();
  const uint16_t tint = scoreTint(score);
  char score_text[8];

  snprintf(score_text, sizeof(score_text), "%u", score);
  target->fillRoundRect(rect.x, rect.y, rect.w, rect.h, 12, panel);
  target->drawRoundRect(rect.x, rect.y, rect.w, rect.h, 12, line);
  drawCenteredTextInRect(target, label,
                         Rect{rect.x, static_cast<int16_t>(rect.y + 6), rect.w, 12}, 1, muted,
                         panel);
  drawCenteredTextInRect(target, score_text,
                         Rect{rect.x, static_cast<int16_t>(rect.y + 20), rect.w, 22}, 2, strong,
                         panel);
  target->fillRoundRect(rect.x + 10, rect.y + rect.h - 18, rect.w - 20, 6, 3,
                        theme::rgb565(24, 56, 70));
  target->fillRoundRect(rect.x + 10, rect.y + rect.h - 18,
                        static_cast<int16_t>((rect.w - 20) * score / 100.0f), 6, 3, tint);
  drawCenteredTextInRect(target, scoreBandLabel(score),
                         Rect{rect.x, static_cast<int16_t>(rect.y + rect.h - 12), rect.w, 10},
                         1, tint, panel);
}

void drawMetricTile(Arduino_GFX *target,
                    const Rect &rect,
                    telemetry::DriveMode mode,
                    const char *label,
                    const char *value,
                    uint16_t value_color) {
  const uint16_t panel = theme::rgb565(10, 24, 36);
  const uint16_t line = lineColor(mode);
  const uint16_t muted = mutedColor();

  target->fillRoundRect(rect.x, rect.y, rect.w, rect.h, 10, panel);
  target->drawRoundRect(rect.x, rect.y, rect.w, rect.h, 10, line);
  drawCenteredTextInRect(target, label,
                         Rect{rect.x, static_cast<int16_t>(rect.y + 6), rect.w, 10}, 1, muted,
                         panel);
  drawCenteredTextInRect(target, value,
                         Rect{rect.x, static_cast<int16_t>(rect.y + 18), rect.w, 16}, 1,
                         value_color,
                         panel);
}

uint16_t quantizeTenths(float value) {
  return static_cast<uint16_t>(constrain(lroundf(max(0.0f, value) * 10.0f), 0L, 65535L));
}

uint16_t quantizeHundredths(float value) {
  return static_cast<uint16_t>(constrain(lroundf(max(0.0f, value) * 100.0f), 0L, 65535L));
}

void drawDashboardScene(const DashboardSnapshot &snapshot,
                        const telemetry::DashboardTelemetry &telemetry,
                        const DashboardViewState &view_state,
                        bool reminder_visible) {
  Arduino_GFX *target = gfx;
  const telemetry::DriveMode mode = snapshot.mode;
  const uint16_t bg = backgroundColor(mode);
  const uint16_t panel = theme::rgb565(7, 18, 30);
  const uint16_t line = theme::rgb565(36, 124, 150);
  const uint16_t speed_fill = theme::rgb565(4, 12, 22);
  const uint16_t strong = theme::rgb565(190, 248, 255);
  const uint16_t muted = mutedColor();
  const uint16_t rail_fill = theme::rgb565(10, 24, 36);
  const uint16_t fuel_accent = theme::rgb565(116, 242, 250);
  const uint16_t dim = theme::rgb565(32, 72, 86);
  const bool low_fuel = snapshot.fuel_level_pct < 15 && snapshot.fuel_level_pct > 0;
  const uint16_t energy_color = low_fuel ? theme::warning() : fuel_accent;
  char energy_value[32];
  snprintf(energy_value, sizeof(energy_value), "%s / %s", snapshot.fuel_text, snapshot.range_text);

  target->fillScreen(bg);
  target->fillRoundRect(kClusterPanel.x, kClusterPanel.y, kClusterPanel.w, kClusterPanel.h, 20, panel);
  target->drawRoundRect(kClusterPanel.x, kClusterPanel.y, kClusterPanel.w, kClusterPanel.h, 20, line);
  drawTopChrome(snapshot, telemetry, view_state);
  if (reminder_visible) {
    drawReminderZone(target, mode);
  }

  target->fillRoundRect(kSpeedPanel.x, kSpeedPanel.y, kSpeedPanel.w, kSpeedPanel.h, 22, speed_fill);
  drawTachRibbon(target, mode, static_cast<float>(snapshot.displayed_rpm));
  drawSpeedBackdrop(target, mode);

  drawCenteredFontTextAt(target, u8g2_font_logisoso92_tn, snapshot.speed_text, kSpeedCenterX,
                         kSpeedCenterY, textColor(), bg);

  target->setFont();
  target->setTextSize(1);
  target->setTextColor(strong, bg);
  target->setCursor(312, 142);
  target->print("KM/H");

  target->fillRoundRect(kDriveRect.x, kDriveRect.y, kDriveRect.w, kDriveRect.h, 8, rail_fill);
  target->drawRoundRect(kDriveRect.x, kDriveRect.y, kDriveRect.w, kDriveRect.h, 8, line);
  target->fillRoundRect(kEnergyRect.x, kEnergyRect.y, kEnergyRect.w, kEnergyRect.h, 8, rail_fill);
  target->drawRoundRect(kEnergyRect.x, kEnergyRect.y, kEnergyRect.w, kEnergyRect.h, 8, line);

  drawCenteredTextInRect(target, "DRIVE", Rect{kDriveRect.x + 8, kDriveRect.y, 34, kDriveRect.h},
                         1, muted, rail_fill);
  drawCenteredTextInRect(target, snapshot.gear_text,
                         Rect{kDriveRect.x + 42, kDriveRect.y, 42, kDriveRect.h},
                         1, textColor(), rail_fill);

  drawCenteredTextInRect(target, "ENERGY",
                         Rect{kEnergyRect.x + 8, kEnergyRect.y, 48, kEnergyRect.h},
                         1, muted, rail_fill);
  drawCenteredTextInRect(target, energy_value,
                         Rect{kEnergyRect.x + 56, kEnergyRect.y, 74, kEnergyRect.h},
                         1, strong, rail_fill);
  target->fillRoundRect(kEnergyRect.x + 132, kEnergyRect.y + 8, 34, 4, 2, dim);
  target->fillRoundRect(kEnergyRect.x + 132, kEnergyRect.y + 8,
                        static_cast<int16_t>(34.0f * snapshot.fuel_fill_pct / 100.0f), 4, 2,
                        energy_color);
}

void drawDetailView(const DashboardSnapshot &snapshot,
                    const telemetry::DashboardTelemetry &telemetry,
                    const DashboardViewState &view_state) {
  const telemetry::DriveMode mode = snapshot.mode;
  const uint16_t bg = backgroundColor(mode);
  const uint16_t text = textColor();
  const uint16_t muted = mutedColor();
  const uint16_t strong = theme::rgb565(210, 252, 255);
  const uint16_t hero_panel = theme::rgb565(7, 18, 30);
  const uint16_t hero_line = lineColor(mode);
  char score_text[8];
  char distance_text[16];
  char avg_economy_text[16];
  char instant_economy_text[16];
  char gph_text[16];
  char fuel_used_text[16];
  char maf_text[16];
  char event_text[16];
  char coaching_line_one[56];
  char coaching_line_two[56];
  const TripMetrics &trip = view_state.trip;

  gfx->fillScreen(bg);
  drawTopChrome(snapshot, telemetry, view_state);

  snprintf(score_text, sizeof(score_text), "%u", trip.trip_score);
  snprintf(distance_text, sizeof(distance_text), "%.1f km", trip.trip_distance_km);
  snprintf(avg_economy_text, sizeof(avg_economy_text), "%.1f L/100", trip.avg_l_per_100km);
  if (telemetry.speed_kph > 0) {
    snprintf(instant_economy_text, sizeof(instant_economy_text), "%.1f L/100",
             trip.instant_l_per_100km);
  } else {
    snprintf(instant_economy_text, sizeof(instant_economy_text), "IDLE");
  }
  snprintf(gph_text, sizeof(gph_text), "%.2f GPH", trip.instant_gph);
  snprintf(fuel_used_text, sizeof(fuel_used_text), "%.2f L", trip.fuel_used_l);
  snprintf(maf_text, sizeof(maf_text), "%.1f g/s", trip.estimated_maf_gps);
  snprintf(event_text, sizeof(event_text), "%u / %u", trip.harsh_acceleration_count,
           trip.harsh_braking_count);
  buildCoachingLines(trip.coaching_message, coaching_line_one, sizeof(coaching_line_one),
                     coaching_line_two, sizeof(coaching_line_two));

  gfx->fillRoundRect(16, 58, 160, 88, 14, hero_panel);
  gfx->drawRoundRect(16, 58, 160, 88, 14, hero_line);
  drawCenteredTextInRect(gfx, "TRIP SCORE", Rect{16, 66, 160, 12}, 1, muted, hero_panel);
  drawCenteredTextInRect(gfx, score_text, Rect{16, 84, 160, 28}, 3, strong, hero_panel);
  drawCenteredTextInRect(gfx, scoreBandLabel(trip.trip_score), Rect{16, 124, 160, 12}, 1,
                         scoreTint(trip.trip_score), hero_panel);

  drawScoreCard(gfx, Rect{190, 58, 84, 88}, mode, "FUEL", trip.fuel_saving_score);
  drawScoreCard(gfx, Rect{286, 58, 84, 88}, mode, "COMFORT", trip.comfort_score);
  drawScoreCard(gfx, Rect{382, 58, 82, 88}, mode, "FLOW", trip.flow_score);

  gfx->fillRoundRect(16, 158, 448, 42, 12, hero_panel);
  gfx->drawRoundRect(16, 158, 448, 42, 12, hero_line);
  gfx->setTextColor(muted, hero_panel);
  gfx->setCursor(28, 173);
  gfx->print("COACH");
  gfx->setTextColor(text, hero_panel);
  gfx->setCursor(92, 173);
  gfx->print(coaching_line_one);
  if (coaching_line_two[0] != '\0') {
    gfx->setCursor(92, 187);
    gfx->print(coaching_line_two);
  }

  drawMetricTile(gfx, Rect{16, 214, 140, 42}, mode, "DISTANCE", distance_text, text);
  drawMetricTile(gfx, Rect{170, 214, 140, 42}, mode, "AVG ECON", avg_economy_text,
                 scoreTint(trip.fuel_saving_score));
  drawMetricTile(gfx, Rect{324, 214, 140, 42}, mode, "INST ECON", instant_economy_text,
                 strong);
  drawMetricTile(gfx, Rect{16, 266, 140, 42}, mode, "FUEL USED", fuel_used_text, text);
  drawMetricTile(gfx, Rect{170, 266, 140, 42}, mode, "INSTANT GPH", gph_text, text);
  drawMetricTile(gfx, Rect{324, 266, 140, 42}, mode, "MAF / EVENTS", maf_text,
                 scoreTint(trip.comfort_score));

  gfx->setTextColor(muted, bg);
  gfx->setCursor(338, 300);
  gfx->print(event_text);

  gfx->setTextColor(muted, bg);
  gfx->setCursor(22, 300);
  if (view_state.telemetry_data_mode == TelemetryDataMode::Fallback) {
    gfx->print("OBD data missing. Fallback drive model active.");
  } else {
    gfx->print("BOOT toggles drive / trip detail");
  }
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

  if (!panel_gfx->begin(kDisplaySpiHz)) {
    log.println("[DISPLAY] panel begin failed.");
    return;
  }

  panel_gfx->setRotation(kDisplayRotation);

  if (framebuffer_gfx == nullptr) {
    framebuffer_gfx = new Arduino_Canvas(kScreenWidth, kScreenHeight, panel_gfx);
    if (framebuffer_gfx != nullptr && framebuffer_gfx->begin(GFX_SKIP_OUTPUT_BEGIN)) {
      gfx = framebuffer_gfx;
      log.println("[DISPLAY] Canvas back buffer enabled.");
    } else {
      delete framebuffer_gfx;
      framebuffer_gfx = nullptr;
      gfx = panel_gfx;
      log.println("[DISPLAY] Canvas unavailable, using direct panel draws.");
    }
  }

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, HIGH);
  gfx->fillScreen(backgroundColor(telemetry::DriveMode::Cruise));
  flushDisplay();

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

  char clock_label[12];
  bool time_synced = false;
  int32_t clock_key = -1;
  int8_t local_hour = -1;
  buildClockLabel(clock_label, sizeof(clock_label), &time_synced, &clock_key, &local_hour);
  const bool mode_changed = !scene_drawn_ || last_mode_ != telemetry.drive_mode;
  const bool reminder_visible = shouldShowHeadlightReminder(telemetry, time_synced, local_hour);

  displayed_rpm_ = smoothDisplayedRpm(displayed_rpm_, telemetry.rpm,
                                      displayed_rpm_ < 0.0f || mode_changed, delta_ms);
  const int16_t displayed_rpm = static_cast<int16_t>(lroundf(displayed_rpm_));
  const DashboardSnapshot snapshot =
      buildDashboardSnapshot(telemetry, view_state, detail_view_, clock_label, time_synced,
                             displayed_rpm);

  if (detail_view_) {
    const TripMetrics &trip = view_state.trip;
    const bool trip_changed = !telemetry_cached_ ||
                              last_trip_distance_tenths_ != quantizeTenths(trip.trip_distance_km) ||
                              last_trip_avg_l100_tenths_ != quantizeTenths(trip.avg_l_per_100km) ||
                              last_trip_instant_l100_tenths_ != quantizeTenths(trip.instant_l_per_100km) ||
                              last_trip_gph_hundredths_ != quantizeHundredths(trip.instant_gph) ||
                              last_trip_fuel_used_hundredths_ != quantizeHundredths(trip.fuel_used_l) ||
                              last_trip_maf_tenths_ != quantizeTenths(trip.estimated_maf_gps) ||
                              last_trip_event_counts_ !=
                                  static_cast<uint16_t>((static_cast<uint16_t>(trip.harsh_acceleration_count) << 8) |
                                                        trip.harsh_braking_count) ||
                              last_trip_fuel_score_ != trip.fuel_saving_score ||
                              last_trip_comfort_score_ != trip.comfort_score ||
                              last_trip_flow_score_ != trip.flow_score ||
                              last_trip_score_ != trip.trip_score ||
                              strcmp(last_trip_message_, trip.coaching_message) != 0;
    const bool detail_header_changed = !telemetry_cached_ ||
                                       last_clock_key_ != clock_key ||
                                       last_time_synced_ != time_synced ||
                                       last_telemetry_data_mode_ != view_state.telemetry_data_mode ||
                                       last_weather_temp_c_ != telemetry.weather_temp_c ||
                                       last_weather_code_ != telemetry.weather_code ||
                                       last_wifi_connected_ != telemetry.wifi_connected ||
                                       last_mode_ != telemetry.drive_mode ||
                                       last_obd_connection_state_ != view_state.obd_connection_state;

    if (trip_changed || detail_header_changed || !last_detail_view_) {
      drawDetailView(snapshot, telemetry, view_state);
      flushDisplay();
    }
    telemetry_cached_ = true;
    last_detail_view_ = true;
    last_speed_kph_ = telemetry.speed_kph;
    last_rpm_ = telemetry.rpm;
    last_fuel_level_pct_ = telemetry.fuel_level_pct;
    last_range_km_ = telemetry.estimated_range_km;
    last_trip_distance_tenths_ = quantizeTenths(trip.trip_distance_km);
    last_trip_avg_l100_tenths_ = quantizeTenths(trip.avg_l_per_100km);
    last_trip_instant_l100_tenths_ = quantizeTenths(trip.instant_l_per_100km);
    last_trip_gph_hundredths_ = quantizeHundredths(trip.instant_gph);
    last_trip_fuel_used_hundredths_ = quantizeHundredths(trip.fuel_used_l);
    last_trip_maf_tenths_ = quantizeTenths(trip.estimated_maf_gps);
    last_trip_event_counts_ =
        static_cast<uint16_t>((static_cast<uint16_t>(trip.harsh_acceleration_count) << 8) |
                              trip.harsh_braking_count);
    last_trip_fuel_score_ = trip.fuel_saving_score;
    last_trip_comfort_score_ = trip.comfort_score;
    last_trip_flow_score_ = trip.flow_score;
    last_trip_score_ = trip.trip_score;
    snprintf(last_trip_message_, sizeof(last_trip_message_), "%s", trip.coaching_message);
    last_telemetry_data_mode_ = view_state.telemetry_data_mode;
    last_gear_ = telemetry.gear;
    last_obd_connection_state_ = view_state.obd_connection_state;
    last_mode_ = telemetry.drive_mode;
    last_welcome_visible_ = view_state.welcome_visible;
    last_clock_key_ = clock_key;
    last_time_synced_ = time_synced;
    last_weather_temp_c_ = telemetry.weather_temp_c;
    last_weather_code_ = telemetry.weather_code;
    last_wifi_connected_ = telemetry.wifi_connected;
    last_nearest_camera_m_ = telemetry.nearest_camera_m;
    return;
  }

  if (last_detail_view_) {
    scene_drawn_ = false;
    telemetry_cached_ = false;
    last_detail_view_ = false;
  }

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
  const bool time_changed = !telemetry_cached_ || last_clock_key_ != clock_key ||
                            last_time_synced_ != time_synced;
  const bool weather_changed = !telemetry_cached_ ||
                               last_weather_temp_c_ != telemetry.weather_temp_c ||
                               last_weather_code_ != telemetry.weather_code ||
                               last_wifi_connected_ != telemetry.wifi_connected;
  const bool data_mode_changed = !telemetry_cached_ ||
                                 last_telemetry_data_mode_ != view_state.telemetry_data_mode;
  const bool camera_changed = !telemetry_cached_ ||
                              last_nearest_camera_m_ != telemetry.nearest_camera_m;
  const bool reminder_changed = !telemetry_cached_ || last_reminder_visible_ != reminder_visible;
  const bool arc_animating = !telemetry_cached_ || displayed_rpm != last_arc_rpm_drawn_;

  if (scene_changed || time_changed || weather_changed || camera_changed || speed_changed ||
      rpm_changed || arc_animating || reminder_changed || gear_changed || range_changed ||
      data_mode_changed ||
      fuel_changed || mode_changed) {
    drawDashboardScene(snapshot, telemetry, view_state, reminder_visible);
    flushDisplay();
    scene_drawn_ = true;
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
  last_clock_key_ = clock_key;
  last_time_synced_ = time_synced;
  last_reminder_visible_ = reminder_visible;
  last_telemetry_data_mode_ = view_state.telemetry_data_mode;
  last_weather_temp_c_ = telemetry.weather_temp_c;
  last_weather_code_ = telemetry.weather_code;
  last_wifi_connected_ = telemetry.wifi_connected;
  last_nearest_camera_m_ = telemetry.nearest_camera_m;
}

}  // namespace app