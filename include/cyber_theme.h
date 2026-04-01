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
  inline uint16_t bg()           { return rgb565(5, 9, 16); }
  inline uint16_t panel()        { return rgb565(10, 16, 34); }
  inline uint16_t line()         { return rgb565(22, 44, 70); }
  inline uint16_t accent()       { return rgb565(134, 255, 225); }
  inline uint16_t accentStrong() { return rgb565(215, 248, 255); }
  inline uint16_t accentDim()    { return rgb565(38, 94, 102); }
  inline uint16_t moodRing()     { return rgb565(113, 207, 255); }
  inline uint16_t accentAlt()    { return rgb565(113, 207, 255); }
  inline uint16_t shift()        { return rgb565(255, 95, 210); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(16, 7, 10); }
  inline uint16_t panel()        { return rgb565(24, 10, 18); }
  inline uint16_t line()         { return rgb565(68, 28, 32); }
  inline uint16_t accent()       { return rgb565(255, 177, 75); }
  inline uint16_t accentStrong() { return rgb565(255, 229, 194); }
  inline uint16_t accentDim()    { return rgb565(104, 58, 30); }
  inline uint16_t moodRing()     { return rgb565(255, 123, 95); }
  inline uint16_t accentAlt()    { return rgb565(255, 123, 95); }
  inline uint16_t shift()        { return rgb565(255, 85, 200); }
}
inline uint16_t text()       { return rgb565(220, 235, 250); }
inline uint16_t muted()      { return rgb565(80, 100, 120); }
inline uint16_t trackChill() { return rgb565(18, 28, 44); }
inline uint16_t trackSport() { return rgb565(38, 18, 20); }
}  // namespace dark

// ── LIGHT mode palette (headlights OFF / daytime, default) ──
// Not white — a slightly lifted dark palette for daytime readability.

namespace light {
namespace chill {
  inline uint16_t bg()           { return rgb565(8, 16, 28); }
  inline uint16_t panel()        { return rgb565(14, 26, 44); }
  inline uint16_t line()         { return rgb565(34, 72, 96); }
  inline uint16_t accent()       { return rgb565(134, 255, 225); }
  inline uint16_t accentStrong() { return rgb565(215, 248, 255); }
  inline uint16_t accentDim()    { return rgb565(46, 118, 126); }
  inline uint16_t moodRing()     { return rgb565(113, 207, 255); }
  inline uint16_t accentAlt()    { return rgb565(113, 207, 255); }
  inline uint16_t shift()        { return rgb565(255, 95, 210); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(24, 12, 10); }
  inline uint16_t panel()        { return rgb565(40, 18, 20); }
  inline uint16_t line()         { return rgb565(92, 46, 38); }
  inline uint16_t accent()       { return rgb565(255, 177, 75); }
  inline uint16_t accentStrong() { return rgb565(255, 229, 194); }
  inline uint16_t accentDim()    { return rgb565(134, 76, 40); }
  inline uint16_t moodRing()     { return rgb565(255, 123, 95); }
  inline uint16_t accentAlt()    { return rgb565(255, 123, 95); }
  inline uint16_t shift()        { return rgb565(255, 85, 200); }
}
inline uint16_t text()       { return rgb565(235, 248, 255); }
inline uint16_t muted()      { return rgb565(110, 140, 160); }
inline uint16_t trackChill() { return rgb565(26, 40, 58); }
inline uint16_t trackSport() { return rgb565(56, 28, 24); }
}  // namespace light

// Common (mode-independent)
inline uint16_t warning() { return rgb565(255, 177, 75); }

// Arc gradient colors (toxic neon — lime to emerald)
inline uint16_t arcStart()  { return rgb565(170, 255, 0); }
inline uint16_t arcMid()    { return rgb565(68, 255, 50); }
inline uint16_t arcHigh()   { return rgb565(0, 255, 102); }
inline uint16_t arcEnd()    { return rgb565(0, 204, 136); }

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
  inline uint16_t accentAlt()    { return darkMode() ? dark::chill::accentAlt()    : light::chill::accentAlt(); }
  inline uint16_t shift()        { return darkMode() ? dark::chill::shift()        : light::chill::shift(); }
}

namespace sport {
  inline uint16_t bg()           { return darkMode() ? dark::sport::bg()           : light::sport::bg(); }
  inline uint16_t panel()        { return darkMode() ? dark::sport::panel()        : light::sport::panel(); }
  inline uint16_t line()         { return darkMode() ? dark::sport::line()          : light::sport::line(); }
  inline uint16_t accent()       { return darkMode() ? dark::sport::accent()       : light::sport::accent(); }
  inline uint16_t accentStrong() { return darkMode() ? dark::sport::accentStrong() : light::sport::accentStrong(); }
  inline uint16_t accentDim()    { return darkMode() ? dark::sport::accentDim()    : light::sport::accentDim(); }
  inline uint16_t moodRing()     { return darkMode() ? dark::sport::moodRing()     : light::sport::moodRing(); }
  inline uint16_t accentAlt()    { return darkMode() ? dark::sport::accentAlt()    : light::sport::accentAlt(); }
  inline uint16_t shift()        { return darkMode() ? dark::sport::shift()        : light::sport::shift(); }
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
inline uint16_t accentAlt(uint8_t dm)    { return isSport(dm) ? sport::accentAlt()    : chill::accentAlt(); }
inline uint16_t shift(uint8_t dm)        { return isSport(dm) ? sport::shift()        : chill::shift(); }
inline uint16_t track(uint8_t dm)        { return isSport(dm) ? (darkMode() ? dark::trackSport() : light::trackSport())
                                                               : (darkMode() ? dark::trackChill() : light::trackChill()); }

}  // namespace theme
