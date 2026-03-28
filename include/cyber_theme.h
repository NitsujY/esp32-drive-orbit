#pragma once

#include <Arduino.h>

namespace theme {

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ── Global light/dark mode flag ──
// Default: false (light mode). Set to true when headlights are detected on.
inline bool &darkMode() {
  static bool dark = false;
  return dark;
}

// ── DARK mode palette (headlights ON / night) ──

namespace dark {
namespace chill {
  inline uint16_t bg()           { return rgb565(2, 4, 10); }
  inline uint16_t panel()        { return rgb565(4, 10, 20); }
  inline uint16_t line()         { return rgb565(12, 30, 45); }
  inline uint16_t accent()       { return rgb565(0, 255, 200); }
  inline uint16_t accentStrong() { return rgb565(180, 255, 240); }
  inline uint16_t accentDim()    { return rgb565(0, 80, 65); }
  inline uint16_t moodRing()     { return rgb565(0, 160, 140); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(12, 2, 4); }
  inline uint16_t panel()        { return rgb565(22, 4, 8); }
  inline uint16_t line()         { return rgb565(50, 12, 20); }
  inline uint16_t accent()       { return rgb565(255, 50, 120); }
  inline uint16_t accentStrong() { return rgb565(255, 180, 210); }
  inline uint16_t accentDim()    { return rgb565(100, 15, 40); }
  inline uint16_t moodRing()     { return rgb565(255, 30, 70); }
}
inline uint16_t text()       { return rgb565(220, 235, 250); }
inline uint16_t muted()      { return rgb565(80, 100, 120); }
inline uint16_t trackChill() { return rgb565(12, 25, 38); }
inline uint16_t trackSport() { return rgb565(38, 12, 20); }
}  // namespace dark

// ── LIGHT mode palette (headlights OFF / daytime, default) ──
// Not white — a slightly lifted dark palette for daytime readability.

namespace light {
namespace chill {
  inline uint16_t bg()           { return rgb565(8, 16, 28); }
  inline uint16_t panel()        { return rgb565(14, 26, 44); }
  inline uint16_t line()         { return rgb565(28, 58, 80); }
  inline uint16_t accent()       { return rgb565(0, 255, 200); }
  inline uint16_t accentStrong() { return rgb565(180, 255, 240); }
  inline uint16_t accentDim()    { return rgb565(0, 120, 100); }
  inline uint16_t moodRing()     { return rgb565(0, 180, 160); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(28, 8, 14); }
  inline uint16_t panel()        { return rgb565(44, 14, 22); }
  inline uint16_t line()         { return rgb565(80, 28, 40); }
  inline uint16_t accent()       { return rgb565(255, 60, 130); }
  inline uint16_t accentStrong() { return rgb565(255, 190, 215); }
  inline uint16_t accentDim()    { return rgb565(130, 25, 55); }
  inline uint16_t moodRing()     { return rgb565(255, 45, 85); }
}
inline uint16_t text()       { return rgb565(235, 248, 255); }
inline uint16_t muted()      { return rgb565(110, 140, 160); }
inline uint16_t trackChill() { return rgb565(24, 46, 62); }
inline uint16_t trackSport() { return rgb565(62, 24, 35); }
}  // namespace light

// Common (mode-independent)
inline uint16_t warning() { return rgb565(255, 200, 60); }

// Arc gradient colors (neon sweep — same in both light/dark)
inline uint16_t arcStart()  { return rgb565(255, 0, 200); }
inline uint16_t arcMid()    { return rgb565(120, 0, 255); }
inline uint16_t arcHigh()   { return rgb565(0, 120, 255); }
inline uint16_t arcEnd()    { return rgb565(0, 255, 220); }

// ── Mode-aware helpers (read darkMode() flag) ──

inline bool isSport(uint8_t drive_mode) { return drive_mode == 2; }

// Chill namespace accessors (light/dark aware)
namespace chill {
  inline uint16_t bg()           { return darkMode() ? dark::chill::bg()           : light::chill::bg(); }
  inline uint16_t panel()        { return darkMode() ? dark::chill::panel()        : light::chill::panel(); }
  inline uint16_t line()         { return darkMode() ? dark::chill::line()          : light::chill::line(); }
  inline uint16_t accent()       { return darkMode() ? dark::chill::accent()       : light::chill::accent(); }
  inline uint16_t accentStrong() { return darkMode() ? dark::chill::accentStrong() : light::chill::accentStrong(); }
  inline uint16_t accentDim()    { return darkMode() ? dark::chill::accentDim()    : light::chill::accentDim(); }
  inline uint16_t moodRing()     { return darkMode() ? dark::chill::moodRing()     : light::chill::moodRing(); }
}

namespace sport {
  inline uint16_t bg()           { return darkMode() ? dark::sport::bg()           : light::sport::bg(); }
  inline uint16_t panel()        { return darkMode() ? dark::sport::panel()        : light::sport::panel(); }
  inline uint16_t line()         { return darkMode() ? dark::sport::line()          : light::sport::line(); }
  inline uint16_t accent()       { return darkMode() ? dark::sport::accent()       : light::sport::accent(); }
  inline uint16_t accentStrong() { return darkMode() ? dark::sport::accentStrong() : light::sport::accentStrong(); }
  inline uint16_t accentDim()    { return darkMode() ? dark::sport::accentDim()    : light::sport::accentDim(); }
  inline uint16_t moodRing()     { return darkMode() ? dark::sport::moodRing()     : light::sport::moodRing(); }
}

inline uint16_t text()  { return darkMode() ? dark::text()  : light::text(); }
inline uint16_t muted() { return darkMode() ? dark::muted() : light::muted(); }

inline uint16_t bg(uint8_t dm)           { return isSport(dm) ? sport::bg()           : chill::bg(); }
inline uint16_t panel(uint8_t dm)        { return isSport(dm) ? sport::panel()        : chill::panel(); }
inline uint16_t line(uint8_t dm)         { return isSport(dm) ? sport::line()          : chill::line(); }
inline uint16_t accent(uint8_t dm)       { return isSport(dm) ? sport::accent()       : chill::accent(); }
inline uint16_t accentStrong(uint8_t dm) { return isSport(dm) ? sport::accentStrong() : chill::accentStrong(); }
inline uint16_t accentDim(uint8_t dm)    { return isSport(dm) ? sport::accentDim()    : chill::accentDim(); }
inline uint16_t moodRing(uint8_t dm)     { return isSport(dm) ? sport::moodRing()     : chill::moodRing(); }
inline uint16_t track(uint8_t dm)        { return isSport(dm) ? (darkMode() ? dark::trackSport() : light::trackSport())
                                                               : (darkMode() ? dark::trackChill() : light::trackChill()); }

}  // namespace theme
