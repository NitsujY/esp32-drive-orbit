#include "dashboard_display.h"

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <TCA9554.h>
#include <Wire.h>

#include <math.h>
#include <stdio.h>
#include <time.h>

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
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kClusterX = 0;
constexpr int16_t kClusterY = 56;
constexpr int16_t kClusterWidth = 480;
constexpr int16_t kClusterHeight = 224;
constexpr int16_t kBottomCardY = 284;
constexpr int16_t kBottomCardWidth = 110;
constexpr int16_t kBottomCardHeight = 36;
constexpr int16_t kBottomCardGap = 10;
constexpr int16_t kBottomCardX0 = 8;
constexpr int16_t kRpmMax = 5000;
constexpr uint8_t kTachSegmentCount = 20;
constexpr uint8_t kTachShiftStart = 16;
constexpr int16_t kTachX = 18;
constexpr int16_t kTachY = 10;
constexpr int16_t kTachWidth = 444;
constexpr int16_t kTachHeight = 52;
constexpr int16_t kSpeedCenterX = kClusterWidth / 2;
constexpr int16_t kSpeedCenterY = 150;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect kSessionBlock{8, 8, 104, 36};
constexpr Rect kSyncBlock{118, 8, 104, 36};
constexpr Rect kClockBlock{228, 8, 124, 36};
constexpr Rect kWeatherBlock{358, 8, 114, 36};
constexpr Rect kTripRegion{112, 214, 256, 14};

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

uint16_t accentAltColor(telemetry::DriveMode mode) {
  return theme::accentAlt(static_cast<uint8_t>(mode));
}

uint16_t shiftColor(telemetry::DriveMode mode) {
  return theme::shift(static_cast<uint8_t>(mode));
}

uint16_t sportRainbowColor(float position) {
  static const uint16_t kRainbowStops[] = {
      theme::rgb565(255, 84, 84),
      theme::rgb565(255, 168, 60),
      theme::rgb565(255, 220, 64),
      theme::rgb565(80, 236, 122),
      theme::rgb565(77, 205, 255),
      theme::rgb565(185, 116, 255),
  };

  const float clamped = constrain(position, 0.0f, 1.0f);
  const float scaled = clamped * static_cast<float>((sizeof(kRainbowStops) / sizeof(kRainbowStops[0])) - 1);
  const uint8_t lower = static_cast<uint8_t>(floorf(scaled));
  const uint8_t upper = min<uint8_t>(lower + 1, (sizeof(kRainbowStops) / sizeof(kRainbowStops[0])) - 1);
  return lerpColor565(kRainbowStops[lower], kRainbowStops[upper], scaled - lower);
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

const char *compactStatusDetail(ObdConnectionState state, bool welcome_visible) {
  if (state == ObdConnectionState::Live) {
    return welcome_visible ? "IGNITION" : "LIVE";
  }

  if (state == ObdConnectionState::Connecting) {
    return "SCANNING";
  }

  return "OBD OFF";
}

const char *screenLabel(DashboardScreen screen) {
  switch (screen) {
    case DashboardScreen::Boot:
      return "Boot";
    case DashboardScreen::Welcome:
      return "Welcome";
    case DashboardScreen::Dashboard:
      return "Drive";
    case DashboardScreen::Trip:
      return "Trip";
    case DashboardScreen::Health:
      return "Health";
    case DashboardScreen::Sport:
      return "Sport";
  }

  return "Drive";
}

void buildClockLabel(char *time_buffer,
                     size_t time_buffer_size,
                     bool *synced_out,
                     int32_t *clock_key_out) {
  struct tm local_time_info {};
  const bool synced = getLocalTime(&local_time_info, 0);

  if (synced) {
    int display_hour = local_time_info.tm_hour % 12;
    if (display_hour == 0) {
      display_hour = 12;
    }
    snprintf(time_buffer, time_buffer_size, "%d:%02d %s", display_hour,
             local_time_info.tm_min, local_time_info.tm_hour >= 12 ? "PM" : "AM");
    if (clock_key_out != nullptr) {
      *clock_key_out = (local_time_info.tm_yday * 1440) +
                       (local_time_info.tm_hour * 60) + local_time_info.tm_min;
    }
  } else {
    snprintf(time_buffer, time_buffer_size, "--:--");
    if (clock_key_out != nullptr) {
      *clock_key_out = -1;
    }
  }

  if (synced_out != nullptr) {
    *synced_out = synced;
  }
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

const char *weatherShortLabel(uint8_t weather_code) {
  if (weather_code == telemetry::kWeatherCodeUnknown) {
    return "--";
  }

  if (weather_code == 0) {
    return "SUN";
  }
  if (weather_code <= 3) {
    return "CLD";
  }
  if (weather_code == 45 || weather_code == 48) {
    return "FOG";
  }
  if ((weather_code >= 51 && weather_code <= 55) ||
      (weather_code >= 61 && weather_code <= 65) ||
      (weather_code >= 80 && weather_code <= 82)) {
    return "RAIN";
  }
  if (weather_code >= 71 && weather_code <= 75) {
    return "SNOW";
  }
  if (weather_code >= 95) {
    return "STM";
  }
  return "WX";
}

void drawWeatherIcon(Arduino_GFX *target,
                     int16_t origin_x,
                     int16_t origin_y,
                     uint8_t weather_code,
                     uint16_t color,
                     uint16_t background) {
  if (weather_code == telemetry::kWeatherCodeUnknown) {
    target->drawCircle(origin_x, origin_y, 7, color);
    target->drawLine(origin_x - 5, origin_y + 5, origin_x + 5, origin_y - 5, color);
    return;
  }

  if (weather_code == 0) {
    target->fillCircle(origin_x, origin_y, 5, color);
    for (int8_t ray = 0; ray < 8; ++ray) {
      const float angle = static_cast<float>(ray) * 45.0f * DEG_TO_RAD;
      const int16_t x0 = origin_x + static_cast<int16_t>(cosf(angle) * 8.0f);
      const int16_t y0 = origin_y + static_cast<int16_t>(sinf(angle) * 8.0f);
      const int16_t x1 = origin_x + static_cast<int16_t>(cosf(angle) * 11.0f);
      const int16_t y1 = origin_y + static_cast<int16_t>(sinf(angle) * 11.0f);
      target->drawLine(x0, y0, x1, y1, color);
    }
    return;
  }

  target->fillCircle(origin_x - 4, origin_y, 4, color);
  target->fillCircle(origin_x + 1, origin_y - 2, 5, color);
  target->fillCircle(origin_x + 6, origin_y, 4, color);
  target->fillRoundRect(origin_x - 8, origin_y, 18, 6, 3, color);

  if ((weather_code >= 51 && weather_code <= 55) ||
      (weather_code >= 61 && weather_code <= 65) ||
      (weather_code >= 80 && weather_code <= 82)) {
    target->drawFastVLine(origin_x - 3, origin_y + 8, 4, color);
    target->drawFastVLine(origin_x + 2, origin_y + 9, 4, color);
    target->drawFastVLine(origin_x + 7, origin_y + 8, 4, color);
  } else if (weather_code >= 71 && weather_code <= 75) {
    target->fillCircle(origin_x - 2, origin_y + 10, 1, color);
    target->fillCircle(origin_x + 3, origin_y + 12, 1, color);
    target->fillCircle(origin_x + 8, origin_y + 10, 1, color);
  } else if (weather_code >= 95) {
    target->fillTriangle(origin_x + 1, origin_y + 7, origin_x + 6, origin_y + 7,
                         origin_x + 2, origin_y + 15, color);
    target->fillTriangle(origin_x + 4, origin_y + 15, origin_x + 8, origin_y + 15,
                         origin_x + 5, origin_y + 21, color);
  } else if (weather_code == 45 || weather_code == 48) {
    target->drawFastHLine(origin_x - 8, origin_y + 9, 18, background);
    target->drawFastHLine(origin_x - 8, origin_y + 10, 18, color);
    target->drawFastHLine(origin_x - 8, origin_y + 13, 18, color);
  }
}

void drawClusterGrid(Arduino_GFX *target, telemetry::DriveMode mode) {
  (void)target;
  (void)mode;
  // Clean background — no pattern.
}

void drawTopChrome(const DashboardViewState &view_state,
                   telemetry::DriveMode mode,
                   bool welcome_visible,
                   const char *clock_label,
                   bool time_synced,
                   int8_t weather_temp_c,
                   uint8_t weather_code,
                   bool wifi_connected) {
  const uint16_t bg = backgroundColor(mode);
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t strong = accentStrongColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));
  const uint16_t muted = mutedColor();

  gfx->fillRect(0, 0, kScreenWidth, 52, bg);

  const char *status_text = nullptr;
  uint16_t status_color = mutedColor();
  if (view_state.obd_connection_state != ObdConnectionState::Live) {
    status_text = compactStatusDetail(view_state.obd_connection_state, welcome_visible);
    status_color = obdStatusColor(view_state.obd_connection_state, mode);
  } else if (welcome_visible) {
    status_text = compactStatusDetail(view_state.obd_connection_state, welcome_visible);
    status_color = accent;
  } else {
    status_text = compactStatusDetail(view_state.obd_connection_state, welcome_visible);
    status_color = accent;
  }

  gfx->fillRoundRect(kSessionBlock.x, kSessionBlock.y, kSessionBlock.w, kSessionBlock.h, 10, panel);
  gfx->drawRoundRect(kSessionBlock.x, kSessionBlock.y, kSessionBlock.w, kSessionBlock.h, 10, line);
  drawCenteredTextInRect(gfx, screenLabel(view_state.active_screen), kSessionBlock, 1, accent, panel);

  gfx->fillRoundRect(kSyncBlock.x, kSyncBlock.y, kSyncBlock.w, kSyncBlock.h, 10, panel);
  gfx->drawRoundRect(kSyncBlock.x, kSyncBlock.y, kSyncBlock.w, kSyncBlock.h, 10, line);
  drawCenteredTextInRect(gfx, status_text, kSyncBlock, 1, status_color, panel);

  gfx->fillRoundRect(kClockBlock.x, kClockBlock.y, kClockBlock.w, kClockBlock.h, 10, panel);
  gfx->drawRoundRect(kClockBlock.x, kClockBlock.y, kClockBlock.w, kClockBlock.h, 10, line);
  drawCenteredTextInRect(gfx, clock_label, kClockBlock, 1, time_synced ? strong : muted, panel);

  gfx->fillRoundRect(kWeatherBlock.x, kWeatherBlock.y, kWeatherBlock.w, kWeatherBlock.h, 12, panel);
  gfx->drawRoundRect(kWeatherBlock.x, kWeatherBlock.y, kWeatherBlock.w, kWeatherBlock.h, 12,
                     wifi_connected ? strong : line);
  const uint16_t weather_color = wifi_connected ? strong : muted;
  drawWeatherIcon(gfx, kWeatherBlock.x + 18, kWeatherBlock.y + 16, weather_code, weather_color,
                  panel);
  char weather_text[16];
  if (weather_temp_c == telemetry::kWeatherTempUnknown) {
    snprintf(weather_text, sizeof(weather_text), "%s", wifi_connected ? "--" : "OFF");
  } else {
    snprintf(weather_text, sizeof(weather_text), "%dC", weather_temp_c);
  }
  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(weather_color, panel);
  gfx->setCursor(kWeatherBlock.x + 38, kWeatherBlock.y + 16);
  gfx->print(weather_text);
  gfx->setTextColor(muted, panel);
  gfx->setCursor(kWeatherBlock.x + 38, kWeatherBlock.y + 28);
  gfx->print(weatherShortLabel(weather_code));

  gfx->drawFastHLine(12, 48, kScreenWidth - 24, dim);
}

void drawSpeedBackdrop(Arduino_GFX *target, telemetry::DriveMode mode) {
  (void)target;
  (void)mode;
}

void drawTachRibbon(Arduino_GFX *target, telemetry::DriveMode mode, float displayed_rpm) {
  const uint16_t bg = backgroundColor(mode);
  const uint16_t line = lineColor(mode);
  const uint16_t accent = accentColor(mode);
  const uint16_t accent_alt = accentAltColor(mode);
  const uint16_t strong = accentStrongColor(mode);
  const uint16_t shift = shiftColor(mode);
  const uint16_t track = trackArcColor(mode);
  const uint8_t active_segments = static_cast<uint8_t>(roundf(constrain(
      displayed_rpm / static_cast<float>(kRpmMax), 0.0f, 1.0f) * kTachSegmentCount));

  char rpm_buffer[8];
  snprintf(rpm_buffer, sizeof(rpm_buffer), "%d", static_cast<int>(displayed_rpm));

  constexpr int16_t kSegmentInsetX = 10;
  constexpr int16_t kSegmentBottomPad = 3;
  constexpr int16_t kSegmentGap = 5;
  const int16_t segment_width =
      (kTachWidth - (kSegmentInsetX * 2) - ((kTachSegmentCount - 1) * kSegmentGap)) /
      kTachSegmentCount;

  target->drawFastHLine(kTachX + kSegmentInsetX, kTachY + kTachHeight - 1,
                        kTachWidth - (kSegmentInsetX * 2), line);

  for (uint8_t index = 0; index < kTachSegmentCount; ++index) {
    const int16_t segment_height = 14 + ((index * 32) / max<uint8_t>(1, kTachSegmentCount - 1));
    const int16_t x = kTachX + kSegmentInsetX + (index * (segment_width + kSegmentGap));
    const int16_t y = kTachY + kTachHeight - kSegmentBottomPad - segment_height;
    uint16_t color = (index >= kTachShiftStart) ? lerpColor565(track, shift, 0.55f) : track;

    if (index < active_segments) {
      if (mode == telemetry::DriveMode::Sport) {
        const float ratio = static_cast<float>(index) /
                            static_cast<float>(max<int>(1, kTachSegmentCount - 1));
        color = sportRainbowColor(ratio);
      } else if (index >= kTachShiftStart) {
        color = shift;
      } else {
        const float mix = static_cast<float>(index) /
                          static_cast<float>(max(1, static_cast<int>(kTachShiftStart - 1)));
        color = lerpColor565(accent, accent_alt, mix);
      }
    }

    target->fillRoundRect(x, y, segment_width, segment_height, 3, color);
  }

  target->setFont();
  target->setTextSize(1);
  target->setTextColor(strong, bg);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  target->getTextBounds(rpm_buffer, 0, 0, &x1, &y1, &width, &height);
  const int16_t rpm_x = kTachX + kTachWidth - 2 - static_cast<int16_t>(width) - x1;
  const int16_t rpm_y = kTachY + kTachHeight + 14;
  target->setCursor(rpm_x, rpm_y);
  target->print(rpm_buffer);
}

void drawCluster(const telemetry::DashboardTelemetry &telemetry,
                 const DashboardViewState &view_state,
                 telemetry::DriveMode mode,
                 float displayed_rpm) {
  Arduino_GFX *target = cluster_canvas;
  const uint16_t bg = backgroundColor(mode);
  const uint16_t strong = accentStrongColor(mode);

  target->fillScreen(bg);
  drawClusterGrid(target, mode);
  drawTachRibbon(target, mode, displayed_rpm);
  drawSpeedBackdrop(target, mode);

  char speed_buffer[8];
  snprintf(speed_buffer, sizeof(speed_buffer), "%d", telemetry.speed_kph);
  drawCenteredFontTextAt(target, u8g2_font_logisoso92_tn, speed_buffer, kSpeedCenterX,
                         kSpeedCenterY - 2, textColor(), bg);
  drawCenteredTextOn(target, "km/h", kSpeedCenterX, kSpeedCenterY + 42, 1, strong, bg);

  char subtitle[48];
  snprintf(subtitle, sizeof(subtitle), "trip %.1f km", view_state.trip.trip_distance_km);
  drawCenteredTextInRect(target, subtitle, kTripRegion, 1, mutedColor(), bg);
  target->flush(true);
}

void drawModeCard(telemetry::DriveMode mode) {
  const uint16_t accent = accentColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));
  const uint16_t panel = panelColor(mode);
  const uint16_t line = lineColor(mode);
  const uint16_t muted = mutedColor();

  // Glassy pill bar background.
  constexpr int16_t kBarX = 8;
  constexpr int16_t kBarY = kBottomCardY;
  constexpr int16_t kBarW = kScreenWidth - 16;
  constexpr int16_t kBarH = kBottomCardHeight;

  gfx->fillRect(0, kBarY - 4, kScreenWidth, kScreenHeight - (kBarY - 4), backgroundColor(mode));
  gfx->fillRoundRect(kBarX, kBarY, kBarW, kBarH, 8, panel);
  gfx->drawRoundRect(kBarX, kBarY, kBarW, kBarH, 8, line);

  // Four equal columns.
  constexpr int16_t kColW = kBarW / 4;
  constexpr int16_t kLabelY = kBarY + 5;
  constexpr int16_t kValueY = kBarY + 17;

  // Dividers.
  for (int i = 1; i < 4; ++i) {
    int16_t dx = kBarX + kColW * i;
    gfx->drawFastVLine(dx, kBarY + 6, kBarH - 12, dim);
  }

  // Column 0: Mode — size 1 instead of 2.
  int16_t cx = kBarX + kColW / 2;
  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(muted, panel);
  gfx->setCursor(cx - 14, kLabelY);
  gfx->print("MODE");
  gfx->setTextSize(1);
  gfx->setTextColor(accent, panel);
  gfx->setCursor(cx - 18, kValueY);
  gfx->print(modeLabel(mode));
}

void drawGearCard(telemetry::TransmissionGear gear, telemetry::DriveMode mode) {
  const uint16_t panel = panelColor(mode);
  const uint16_t text = textColor();
  const uint16_t muted = mutedColor();

  constexpr int16_t kBarX = 8;
  constexpr int16_t kBarW = (kScreenWidth - 16) / 4;
  constexpr int16_t kBarY = kBottomCardY;
  constexpr int16_t kLabelY = kBarY + 5;
  constexpr int16_t kValueY = kBarY + 17;
  int16_t cx = kBarX + kBarW + kBarW / 2;

  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(muted, panel);
  gfx->setCursor(cx - 14, kLabelY);
  gfx->print("GEAR");
  gfx->setTextSize(1);
  gfx->setTextColor(text, panel);
  gfx->setCursor(cx - 8, kValueY);
  gfx->print(gearLabel(gear));
}

void drawRangeCard(uint16_t range_km, telemetry::DriveMode mode) {
  const uint16_t panel = panelColor(mode);
  const uint16_t muted = mutedColor();
  const uint16_t strong = accentStrongColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));

  constexpr int16_t kBarX = 8;
  constexpr int16_t kBarW = (kScreenWidth - 16) / 4;
  constexpr int16_t kBarY = kBottomCardY;
  constexpr int16_t kLabelY = kBarY + 5;
  constexpr int16_t kValueY = kBarY + 17;
  int16_t cx = kBarX + kBarW * 2 + kBarW / 2;

  char buf[16];
  snprintf(buf, sizeof(buf), "%ukm", range_km);

  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(muted, panel);
  gfx->setCursor(cx - 18, kLabelY);
  gfx->print("RANGE");
  gfx->setTextColor(strong, panel);
  gfx->setCursor(cx - 18, kValueY);
  gfx->print(buf);

  // Meter bar below text.
  constexpr int16_t kMeterW = 50;
  constexpr int16_t kMeterH = 2;
  constexpr uint16_t kMaxRange = 360;
  int16_t mx = cx - kMeterW / 2;
  int16_t my = kValueY + 12;
  uint16_t fill_w = static_cast<uint16_t>(constrain(
      static_cast<long>(range_km) * kMeterW / kMaxRange, 0L, static_cast<long>(kMeterW)));
  gfx->fillRoundRect(mx, my, kMeterW, kMeterH, 1, dim);
  if (fill_w > 0) {
    gfx->fillRoundRect(mx, my, fill_w, kMeterH, 1, strong);
  }
}

void drawFuelCard(uint8_t fuel_pct, telemetry::DriveMode mode) {
  const uint16_t panel = panelColor(mode);
  const uint16_t muted = mutedColor();
  const bool low_fuel = fuel_pct < 15 && fuel_pct > 0;
  const uint16_t color = low_fuel ? theme::warning() : accentColor(mode);
  const uint16_t dim = theme::accentDim(static_cast<uint8_t>(mode));

  constexpr int16_t kBarX = 8;
  constexpr int16_t kBarW = (kScreenWidth - 16) / 4;
  constexpr int16_t kBarY = kBottomCardY;
  constexpr int16_t kLabelY = kBarY + 5;
  constexpr int16_t kValueY = kBarY + 17;
  int16_t cx = kBarX + kBarW * 3 + kBarW / 2;

  char buf[16];
  snprintf(buf, sizeof(buf), "%u%%", fuel_pct);

  gfx->setFont();
  gfx->setTextSize(1);
  gfx->setTextColor(muted, panel);
  gfx->setCursor(cx - 14, kLabelY);
  gfx->print("FUEL");
  gfx->setTextColor(color, panel);
  gfx->setCursor(cx - 14, kValueY);
  gfx->print(buf);
  if (low_fuel) {
    gfx->setCursor(cx + 14, kValueY);
    gfx->print("LOW");
  }

  // Meter bar below text.
  constexpr int16_t kMeterW = 50;
  constexpr int16_t kMeterH = 2;
  int16_t mx = cx - kMeterW / 2;
  int16_t my = kValueY + 12;
  uint16_t fill_w = static_cast<uint16_t>(constrain(
      static_cast<long>(fuel_pct) * kMeterW / 100L, 0L, static_cast<long>(kMeterW)));
  gfx->fillRoundRect(mx, my, kMeterW, kMeterH, 1, dim);
  if (fill_w > 0) {
    gfx->fillRoundRect(mx, my, fill_w, kMeterH, 1, color);
  }
}

void drawStaticScene(const DashboardViewState &view_state,
                     const telemetry::DashboardTelemetry &telemetry) {
  gfx->fillScreen(backgroundColor(telemetry.drive_mode));
  drawTopChrome(view_state, telemetry.drive_mode, view_state.welcome_visible, "--:--", false,
                telemetry.weather_temp_c, telemetry.weather_code, telemetry.wifi_connected);
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

  char clock_label[12];
  bool time_synced = false;
  int32_t clock_key = -1;
  buildClockLabel(clock_label, sizeof(clock_label), &time_synced, &clock_key);

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
    last_clock_key_ = clock_key;
    last_time_synced_ = time_synced;
    last_weather_temp_c_ = telemetry.weather_temp_c;
    last_weather_code_ = telemetry.weather_code;
    last_wifi_connected_ = telemetry.wifi_connected;
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
  const bool time_changed = !telemetry_cached_ || last_clock_key_ != clock_key ||
                            last_time_synced_ != time_synced;
  const bool weather_changed = !telemetry_cached_ ||
                               last_weather_temp_c_ != telemetry.weather_temp_c ||
                               last_weather_code_ != telemetry.weather_code ||
                               last_wifi_connected_ != telemetry.wifi_connected;

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

  if (scene_changed || time_changed || weather_changed) {
    drawTopChrome(view_state, telemetry.drive_mode, view_state.welcome_visible, clock_label,
                  time_synced, telemetry.weather_temp_c, telemetry.weather_code,
                  telemetry.wifi_connected);
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
  last_clock_key_ = clock_key;
  last_time_synced_ = time_synced;
  last_weather_temp_c_ = telemetry.weather_temp_c;
  last_weather_code_ = telemetry.weather_code;
  last_wifi_connected_ = telemetry.wifi_connected;
}

}  // namespace app