#import <AppKit/AppKit.h>

#include <cmath>
#include <ctime>

#include "cyber_theme.h"
#include "dashboard_scene.h"
#include "dashboard_state.h"
#include "shared_data.h"
#include "vehicle_profiles/vehicle_profile.h"

namespace {

constexpr CGFloat kScale = 2.0;
constexpr CGFloat kCanvasWidth = 480.0;
constexpr CGFloat kCanvasHeight = 320.0;
constexpr CGFloat kWindowWidth = kCanvasWidth * kScale;
constexpr CGFloat kWindowHeight = kCanvasHeight * kScale;
constexpr int16_t kClusterX = 10;
constexpr int16_t kClusterY = 12;
constexpr int16_t kClusterW = 460;
constexpr int16_t kClusterH = 296;
constexpr int16_t kSpeedPanelX = 74;
constexpr int16_t kSpeedPanelY = 56;
constexpr int16_t kSpeedPanelW = 332;
constexpr int16_t kSpeedPanelH = 138;
constexpr int16_t kTachX = 42;
constexpr int16_t kTachY = 208;
constexpr int16_t kTachW = 396;
constexpr int16_t kTachH = 58;
constexpr uint8_t kTachSegmentCount = 20;
constexpr int16_t kRpmMax = 5000;

NSColor *colorFrom565(uint16_t color) {
  const CGFloat red = static_cast<CGFloat>((color >> 11) & 0x1F) / 31.0;
  const CGFloat green = static_cast<CGFloat>((color >> 5) & 0x3F) / 63.0;
  const CGFloat blue = static_cast<CGFloat>(color & 0x1F) / 31.0;
  return [NSColor colorWithSRGBRed:red green:green blue:blue alpha:1.0];
}

NSFont *smallFont() {
  return [NSFont fontWithName:@"Avenir Next Condensed Medium" size:11.0] ?:
         [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
}

NSFont *metaFont() {
  return [NSFont fontWithName:@"Avenir Next Condensed Demi Bold" size:10.0] ?:
         [NSFont systemFontOfSize:10.0 weight:NSFontWeightSemibold];
}

NSFont *labelFont() {
  return [NSFont fontWithName:@"Avenir Next Condensed Demi Bold" size:12.0] ?:
         [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
}

NSFont *speedFont() {
  return [NSFont fontWithName:@"Avenir Next Condensed Heavy" size:92.0] ?:
    [NSFont monospacedDigitSystemFontOfSize:92.0 weight:NSFontWeightBold];
}

NSFont *speedUnitFont() {
  return [NSFont fontWithName:@"Avenir Next Condensed Demi Bold" size:18.0] ?:
    [NSFont systemFontOfSize:18.0 weight:NSFontWeightSemibold];
}

NSDictionary *attributes(NSFont *font, NSColor *color, NSTextAlignment alignment = NSTextAlignmentLeft) {
  NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
  style.alignment = alignment;
  return @{
    NSFontAttributeName: font,
    NSForegroundColorAttributeName: color,
    NSParagraphStyleAttributeName: style,
  };
}

void fillRoundedRect(NSRect rect, CGFloat radius, NSColor *fill, NSColor *stroke = nil) {
  NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
  [fill setFill];
  [path fill];
  if (stroke != nil) {
    [stroke setStroke];
    [path stroke];
  }
}

void drawText(NSString *text, NSPoint point, NSDictionary *attrs) {
  [text drawAtPoint:point withAttributes:attrs];
}

void drawCenteredText(NSString *text, NSRect rect, NSDictionary *attrs) {
  [text drawInRect:rect withAttributes:attrs];
}

void strokeLine(CGFloat x1, CGFloat y1, CGFloat x2, CGFloat y2, NSColor *color, CGFloat width = 1.0) {
  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:NSMakePoint(x1, y1)];
  [path lineToPoint:NSMakePoint(x2, y2)];
  path.lineWidth = width;
  [color setStroke];
  [path stroke];
}

void fillQuad(NSPoint a, NSPoint b, NSPoint c, NSPoint d, NSColor *fill, NSColor *stroke = nil) {
  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:a];
  [path lineToPoint:b];
  [path lineToPoint:c];
  [path lineToPoint:d];
  [path closePath];
  [fill setFill];
  [path fill];
  if (stroke != nil) {
    [stroke setStroke];
    [path stroke];
  }
}

float lerp(float from, float to, float t) {
  return from + (to - from) * t;
}

uint16_t lerp565(uint16_t from, uint16_t to, float t) {
  const float clamped = constrain(t, 0.0f, 1.0f);
  const uint8_t from_r = static_cast<uint8_t>((from >> 11) & 0x1F);
  const uint8_t from_g = static_cast<uint8_t>((from >> 5) & 0x3F);
  const uint8_t from_b = static_cast<uint8_t>(from & 0x1F);
  const uint8_t to_r = static_cast<uint8_t>((to >> 11) & 0x1F);
  const uint8_t to_g = static_cast<uint8_t>((to >> 5) & 0x3F);
  const uint8_t to_b = static_cast<uint8_t>(to & 0x1F);
  return static_cast<uint16_t>((static_cast<uint8_t>(lerp(from_r, to_r, clamped)) << 11) |
                               (static_cast<uint8_t>(lerp(from_g, to_g, clamped)) << 5) |
                               static_cast<uint8_t>(lerp(from_b, to_b, clamped)));
}

uint16_t sportRibbonColor(float position, uint16_t accent, uint16_t accentStrong, uint16_t accentHot) {
  const float clamped = constrain(position, 0.0f, 1.0f);
  const float warmup = constrain(clamped / 0.72f, 0.0f, 1.0f);
  const uint16_t base = lerp565(accentStrong, accent, warmup);
  const float hot = clamped <= 0.68f ? 0.0f : constrain((clamped - 0.68f) / 0.32f, 0.0f, 1.0f);
  return lerp565(base, accentHot, hot);
}

NSString *driveModeText(telemetry::DriveMode mode) {
  switch (mode) {
    case telemetry::DriveMode::Calm:
      return @"CALM";
    case telemetry::DriveMode::Cruise:
      return @"CRUISE";
    case telemetry::DriveMode::Sport:
      return @"SPORT";
  }

  return @"CRUISE";
}

NSString *drivePositionText(const char *gear_text) {
  if (gear_text == nullptr || gear_text[0] == '\0') {
    return @"P";
  }
  return gear_text[0] == 'P' ? @"P" : @"D";
}

NSString *weatherSummaryText(uint8_t weather_code) {
  if (weather_code == telemetry::kWeatherCodeUnknown) {
    return @"CLEAR";
  }

  return weather_code < 50 ? @"FAIR" : @"RAIN";
}

struct PreviewSimulation {
  telemetry::DashboardTelemetry telemetry = telemetry::makeSimulatedTelemetry(0, 0, 3900, 128, 82, 13800, 70);
  app::DashboardViewState view_state = app::makeInitialDashboardViewState();
  float displayed_rpm = -1.0f;
  bool detail_view = false;
  bool paused = false;
  bool night_mode = false;

  void reset() {
    telemetry = telemetry::makeSimulatedTelemetry(0, 0, 3900, 128, 82, 13800, 70);
    telemetry.weather_temp_c = 24;
    telemetry.wifi_connected = true;
    telemetry.nearest_camera_m = telemetry::kNearestCameraUnknown;
    telemetry.headlights_on = night_mode;
    telemetry::refreshDerivedTelemetry(telemetry);
    view_state = app::makeInitialDashboardViewState();
    displayed_rpm = -1.0f;
    detail_view = false;
  }

  void advance(uint32_t delta_ms) {
    if (paused) {
      return;
    }

    const int16_t previous_speed = telemetry.speed_kph;
    telemetry.sequence += 1;
    telemetry.uptime_ms += delta_ms;

    const double t = static_cast<double>(telemetry.uptime_ms) / 1000.0;
    const double speed_wave = 118.0 + 9.0 * std::sin(t / 3.4) + 4.0 * std::sin(t / 1.5);
    telemetry.speed_kph = static_cast<int16_t>(constrain(static_cast<int>(lround(speed_wave)), 96, 138));

    if (telemetry.speed_kph == 0) {
      telemetry.rpm = 0;
    } else {
      const double burst = std::max(0.0, std::sin(t * 1.4));
      telemetry.rpm = static_cast<int16_t>(constrain(
          static_cast<int>(lround(1240.0 + telemetry.speed_kph * 20.0 + burst * 280.0)), 2800, 4700));
    }

    telemetry.coolant_temp_c = static_cast<int16_t>(82 + lround(2.0 * std::sin(t / 8.0)));
    telemetry.battery_mv = static_cast<uint16_t>(13750 + lround(140.0 * std::sin(t / 9.5)));
    telemetry.fuel_level_pct = 70;
    telemetry.weather_temp_c = 24;
    telemetry.wifi_connected = true;
    telemetry.nearest_camera_m = telemetry::kNearestCameraUnknown;
    telemetry.headlights_on = night_mode;

    app::ObdConnectionState next_state = app::ObdConnectionState::Live;
    if (t < 1.0) {
      next_state = app::ObdConnectionState::Disconnected;
    } else if (t < 3.2) {
      next_state = app::ObdConnectionState::Connecting;
    }

    if (view_state.obd_connection_state != app::ObdConnectionState::Live &&
        next_state == app::ObdConnectionState::Live) {
      app::resetTripMetrics(view_state, "Live preview connected. Trip tracking reset.");
    }

    if (next_state == app::ObdConnectionState::Live) {
      view_state.trip.trip_distance_km +=
          static_cast<float>(telemetry.speed_kph) * (static_cast<float>(delta_ms) / 3600000.0f);
    }

    app::vehicle_profiles::refreshTelemetry(telemetry);
    theme::darkMode() = night_mode;
    app::updateDashboardViewState(view_state, telemetry, previous_speed);
    view_state.obd_connection_state = next_state;
    displayed_rpm = app::smoothDisplayedRpm(displayed_rpm, telemetry.rpm,
                                            displayed_rpm < 0.0f, delta_ms);
  }

  app::DashboardSnapshot snapshot() const {
    std::time_t now = std::time(nullptr);
    std::tm local_time_info {};
    localtime_r(&now, &local_time_info);
    char clock_label[16];
    app::buildDashboardClockLabel(local_time_info.tm_hour, local_time_info.tm_min, true,
                                  clock_label, sizeof(clock_label));
    return app::buildDashboardSnapshot(telemetry, view_state, detail_view, clock_label, true,
                                       static_cast<int16_t>(lroundf(displayed_rpm)));
  }
};

}  // namespace

@interface DashboardPreviewView : NSView
- (void)drawPanel:(NSRect)rect mode:(telemetry::DriveMode)mode fill:(uint16_t)fillColor;
- (void)drawTachRibbon:(const app::DashboardSnapshot &)snapshot;
- (void)drawTopMeta:(const app::DashboardSnapshot &)snapshot;
- (void)drawMinimalCluster:(const app::DashboardSnapshot &)snapshot;
@end

@implementation DashboardPreviewView {
  NSTimer *_timer;
  PreviewSimulation _simulation;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self != nil) {
    _timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 30.0)
                                              target:self
                                            selector:@selector(handleTick:)
                                            userInfo:nil
                                             repeats:YES];
  }
  return self;
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self.window makeFirstResponder:self];
}

- (void)handleTick:(NSTimer *)timer {
  (void)timer;
  _simulation.advance(33);
  [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event {
  NSString *characters = event.charactersIgnoringModifiers.lowercaseString;
  if (characters.length == 0) {
    return;
  }

  const unichar key = [characters characterAtIndex:0];
  if (key == 'n') {
    _simulation.night_mode = !_simulation.night_mode;
  } else if (key == 'r') {
    _simulation.reset();
  } else if (key == ' ') {
    _simulation.paused = !_simulation.paused;
  }

  [self setNeedsDisplay:YES];
}

- (void)drawPanel:(NSRect)rect mode:(telemetry::DriveMode)mode fill:(uint16_t)fillColor {
  fillRoundedRect(rect, 18.0, colorFrom565(fillColor),
                  colorFrom565(theme::line(static_cast<uint8_t>(mode))));
  [colorFrom565(theme::accentDim(static_cast<uint8_t>(mode))) setFill];
  NSRectFill(NSMakeRect(rect.origin.x + 20, rect.origin.y + 12, rect.size.width - 40, 1));
}

- (void)drawTopMeta:(const app::DashboardSnapshot &)snapshot {
  const uint16_t muted = theme::rgb565(112, 202, 214);
  const uint16_t orange = theme::rgb565(255, 164, 94);
  const uint16_t strong = theme::rgb565(226, 249, 255);
  NSString *weatherValue = weatherSummaryText(_simulation.telemetry.weather_code);
  NSString *uvValue = @"UV 7";
  NSString *tempValue = [NSString stringWithFormat:@"%dC", _simulation.telemetry.weather_temp_c];

  std::time_t now = std::time(nullptr);
  std::tm local_time_info {};
  localtime_r(&now, &local_time_info);
  NSString *timeValue = [NSString stringWithFormat:@"%02d:%02d", local_time_info.tm_hour,
                                                      local_time_info.tm_min];

  drawText(@"WEATHER:", NSMakePoint(22, 18), attributes(metaFont(), colorFrom565(muted)));
  drawText(weatherValue, NSMakePoint(76, 18), attributes(metaFont(), colorFrom565(strong)));
  drawText(uvValue, NSMakePoint(176, 18), attributes(metaFont(), colorFrom565(orange)));
  drawText(tempValue, NSMakePoint(250, 18), attributes(metaFont(), colorFrom565(strong)));
  drawText(timeValue, NSMakePoint(402, 18), attributes(metaFont(), colorFrom565(strong)));
}

- (void)drawTachRibbon:(const app::DashboardSnapshot &)snapshot {
  const uint16_t line = theme::rgb565(84, 226, 244);
  const uint16_t accent = theme::rgb565(116, 242, 250);
  const uint16_t strong = theme::rgb565(210, 252, 255);
  const uint16_t accentAlt = theme::rgb565(34, 196, 226);
  const uint16_t track = theme::rgb565(25, 60, 80);
  const uint16_t redline = theme::rgb565(255, 154, 94);
  const uint8_t activeSegments = static_cast<uint8_t>(roundf(
      constrain(snapshot.displayed_rpm / static_cast<float>(kRpmMax), 0.0f, 1.0f) * kTachSegmentCount));

  const int16_t segmentInsetX = 0;
  const int16_t baselineY = kTachY + kTachH - 14;
  const int16_t segmentGap = 4;
  const int16_t segmentWidth =
      (kTachW - (segmentInsetX * 2) - ((kTachSegmentCount - 1) * segmentGap)) / kTachSegmentCount;
  const CGFloat slant = 4.0;

  strokeLine(kTachX - 8, baselineY + 6, kTachX + kTachW + 6, baselineY + 6, colorFrom565(line));
  strokeLine(kTachX - 2, baselineY + 2, kTachX - 2, baselineY + 10, colorFrom565(strong));
  strokeLine(kTachX + 150, baselineY + 3, kTachX + 150, baselineY + 9, colorFrom565(strong));
  strokeLine(kTachX + 284, baselineY + 3, kTachX + 284, baselineY + 9, colorFrom565(strong));
  strokeLine(kTachX + kTachW - 8, baselineY + 4, kTachX + kTachW + 18, baselineY + 4,
             colorFrom565(redline), 2.0);

  for (uint8_t index = 0; index < kTachSegmentCount; ++index) {
    const int16_t segmentHeight = 18 + ((index * 26) / max<uint8_t>(1, kTachSegmentCount - 1));
    const int16_t x = kTachX + segmentInsetX + (index * (segmentWidth + segmentGap));
    const int16_t y = baselineY - segmentHeight;
    uint16_t color = track;

    if (index < activeSegments) {
      const float mix = static_cast<float>(index) /
                        static_cast<float>(max(1, static_cast<int>(kTachSegmentCount - 1)));
      color = lerp565(accent, accentAlt, mix * 0.72f);
    }

    fillQuad(NSMakePoint(x, y),
             NSMakePoint(x + segmentWidth, y),
             NSMakePoint(x + segmentWidth + slant, baselineY),
             NSMakePoint(x + slant, baselineY),
             colorFrom565(color));
  }

  drawCenteredText(@"RPM", NSMakeRect(kTachX + 120, baselineY + 12, 160, 18),
                   attributes(labelFont(), colorFrom565(strong), NSTextAlignmentCenter));
}

- (void)drawMinimalCluster:(const app::DashboardSnapshot &)snapshot {
  const uint16_t bg = _simulation.night_mode ? theme::rgb565(3, 8, 16) : theme::rgb565(6, 14, 24);
  const uint16_t panel = theme::rgb565(7, 18, 30);
  const uint16_t text = theme::rgb565(225, 245, 255);
  const uint16_t speedPanelFill = theme::rgb565(4, 12, 22);
  const uint16_t line = theme::rgb565(36, 124, 150);
  const uint16_t accentDim = theme::rgb565(22, 118, 146);
  const uint16_t accentSoft = theme::rgb565(22, 86, 110);
  const uint16_t strong = theme::rgb565(190, 248, 255);
  const uint16_t gearFill = theme::rgb565(14, 30, 42);
  const int16_t speedCenterX = 240;
  const NSRect clusterRect = NSMakeRect(kClusterX, kClusterY, kClusterW, kClusterH);
  const NSRect speedWindowRect = NSMakeRect(kSpeedPanelX, kSpeedPanelY, kSpeedPanelW, kSpeedPanelH);
  const NSRect gearRect = NSMakeRect(kSpeedPanelX + kSpeedPanelW - 84, kSpeedPanelY + 88, 52, 28);
  NSString *gearValue = [NSString stringWithUTF8String:snapshot.gear_text];

  [[colorFrom565(bg) colorWithAlphaComponent:1.0] setFill];
  NSRectFill(NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight));

  fillRoundedRect(clusterRect, 20.0, colorFrom565(panel), colorFrom565(line));
  strokeLine(kClusterX + 20, kClusterY + 22, kClusterX + kClusterW - 20, kClusterY + 22,
             colorFrom565(accentDim), 1.4);

  [self drawTopMeta:snapshot];

  fillRoundedRect(speedWindowRect, 22.0, colorFrom565(speedPanelFill), colorFrom565(line));

  fillQuad(NSMakePoint(kSpeedPanelX - 38, kSpeedPanelY + 28),
           NSMakePoint(kSpeedPanelX - 8, kSpeedPanelY + 28),
           NSMakePoint(kSpeedPanelX - 20, kSpeedPanelY + 58),
           NSMakePoint(kSpeedPanelX - 52, kSpeedPanelY + 58),
           colorFrom565(accentSoft));
  fillQuad(NSMakePoint(kSpeedPanelX + kSpeedPanelW + 8, kSpeedPanelY + 28),
           NSMakePoint(kSpeedPanelX + kSpeedPanelW + 38, kSpeedPanelY + 28),
           NSMakePoint(kSpeedPanelX + kSpeedPanelW + 52, kSpeedPanelY + 58),
           NSMakePoint(kSpeedPanelX + kSpeedPanelW + 20, kSpeedPanelY + 58),
           colorFrom565(accentSoft));

  strokeLine(kSpeedPanelX + 26, kSpeedPanelY + 48, kSpeedPanelX + 106, kSpeedPanelY + 48,
             colorFrom565(accentDim), 1.4);
  strokeLine(kSpeedPanelX + kSpeedPanelW - 106, kSpeedPanelY + 48,
             kSpeedPanelX + kSpeedPanelW - 26, kSpeedPanelY + 48, colorFrom565(accentDim), 1.4);
  strokeLine(kSpeedPanelX + 26, kSpeedPanelY + kSpeedPanelH - 36, kSpeedPanelX + 82,
             kSpeedPanelY + kSpeedPanelH - 36, colorFrom565(accentDim), 1.4);
  strokeLine(kSpeedPanelX + kSpeedPanelW - 82, kSpeedPanelY + kSpeedPanelH - 36,
             kSpeedPanelX + kSpeedPanelW - 26, kSpeedPanelY + kSpeedPanelH - 36,
             colorFrom565(accentDim), 1.4);

  [self drawTachRibbon:snapshot];

  drawCenteredText([NSString stringWithUTF8String:snapshot.speed_text],
                   NSMakeRect(speedCenterX - 130, kSpeedPanelY + 18, 212, 90),
                   attributes(speedFont(), colorFrom565(text), NSTextAlignmentCenter));
  drawText(@"KM/H", NSMakePoint(speedCenterX + 72, kSpeedPanelY + 86),
           attributes(speedUnitFont(), colorFrom565(strong)));

  fillRoundedRect(gearRect, 10.0, colorFrom565(gearFill), colorFrom565(line));
  drawCenteredText(gearValue, NSMakeRect(gearRect.origin.x, gearRect.origin.y + 4,
                                         gearRect.size.width, 18),
                   attributes(labelFont(), colorFrom565(strong), NSTextAlignmentCenter));
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  const app::DashboardSnapshot snapshot = _simulation.snapshot();

  CGContextRef context = NSGraphicsContext.currentContext.CGContext;
  CGContextSaveGState(context);
  CGContextScaleCTM(context, kScale, kScale);

  [[NSColor blackColor] setFill];
  NSRectFill(self.bounds);

  [self drawMinimalCluster:snapshot];

  CGContextRestoreGState(context);
}

@end

@interface PreviewAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation PreviewAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  (void)sender;
  return YES;
}

@end

int main(int argc, const char *argv[]) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    PreviewAppDelegate *delegate = [[PreviewAppDelegate alloc] init];
    application.delegate = delegate;
    [application setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, kWindowWidth, kWindowHeight)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"Smart JDM Dashboard Native Preview - Sport Reference";
    window.minSize = NSMakeSize(kWindowWidth, kWindowHeight);

    DashboardPreviewView *view = [[DashboardPreviewView alloc]
        initWithFrame:NSMakeRect(0, 0, kWindowWidth, kWindowHeight)];
    [window setContentView:view];
    [window center];
    [window makeKeyAndOrderFront:nil];
    [application activateIgnoringOtherApps:YES];
    [application run];
  }

  return 0;
}