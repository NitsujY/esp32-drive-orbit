#pragma once

#include <Arduino.h>

namespace theme {

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ── Global theme flag ──
// false = dark mode  (deep navy tint — default)
// true  = black mode (pure black — for dark car interiors)
inline bool &blackMode() {
  static bool black = false;
  return black;
}

// Keep darkMode() as an alias so existing call-sites compile unchanged.
inline bool &darkMode() { return blackMode(); }

// ── DARK mode palette (deep navy tint — default) ──

namespace dark {
namespace chill {
  inline uint16_t bg()           { return rgb565(12, 18, 28); }
  inline uint16_t panel()        { return rgb565(18, 28, 50); }
  inline uint16_t line()         { return rgb565(30, 56, 84); }
  inline uint16_t accent()       { return rgb565(134, 255, 225); }
  inline uint16_t accentStrong() { return rgb565(215, 248, 255); }
  inline uint16_t accentDim()    { return rgb565(44, 106, 116); }
  inline uint16_t moodRing()     { return rgb565(113, 207, 255); }
  inline uint16_t accentAlt()    { return rgb565(113, 207, 255); }
  inline uint16_t shift()        { return rgb565(255, 95, 210); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(24, 12, 16); }
  inline uint16_t panel()        { return rgb565(36, 16, 26); }
  inline uint16_t line()         { return rgb565(80, 36, 42); }
  inline uint16_t accent()       { return rgb565(255, 177, 75); }
  inline uint16_t accentStrong() { return rgb565(255, 229, 194); }
  inline uint16_t accentDim()    { return rgb565(116, 68, 36); }
  inline uint16_t moodRing()     { return rgb565(255, 123, 95); }
  inline uint16_t accentAlt()    { return rgb565(255, 123, 95); }
  inline uint16_t shift()        { return rgb565(255, 85, 200); }
}
inline uint16_t text()       { return rgb565(220, 235, 250); }
inline uint16_t muted()      { return rgb565(96, 118, 140); }
inline uint16_t trackChill() { return rgb565(24, 36, 56); }
inline uint16_t trackSport() { return rgb565(48, 24, 28); }
}  // namespace dark

// ── BLACK mode palette (pure black — maximum contrast for dark interiors) ──
// Backgrounds go to true black. Accents stay vivid.

namespace black {
namespace chill {
  inline uint16_t bg()           { return rgb565(0, 0, 0); }
  inline uint16_t panel()        { return rgb565(6, 6, 10); }
  inline uint16_t line()         { return rgb565(18, 28, 40); }
  inline uint16_t accent()       { return rgb565(134, 255, 225); }
  inline uint16_t accentStrong() { return rgb565(215, 248, 255); }
  inline uint16_t accentDim()    { return rgb565(28, 72, 80); }
  inline uint16_t moodRing()     { return rgb565(113, 207, 255); }
  inline uint16_t accentAlt()    { return rgb565(113, 207, 255); }
  inline uint16_t shift()        { return rgb565(255, 95, 210); }
}
namespace sport {
  inline uint16_t bg()           { return rgb565(0, 0, 0); }
  inline uint16_t panel()        { return rgb565(10, 4, 6); }
  inline uint16_t line()         { return rgb565(50, 16, 18); }
  inline uint16_t accent()       { return rgb565(255, 177, 75); }
  inline uint16_t accentStrong() { return rgb565(255, 229, 194); }
  inline uint16_t accentDim()    { return rgb565(80, 36, 14); }
  inline uint16_t moodRing()     { return rgb565(255, 123, 95); }
  inline uint16_t accentAlt()    { return rgb565(255, 123, 95); }
  inline uint16_t shift()        { return rgb565(255, 85, 200); }
}
inline uint16_t text()       { return rgb565(230, 240, 255); }
inline uint16_t muted()      { return rgb565(70, 88, 106); }
inline uint16_t trackChill() { return rgb565(10, 16, 26); }
inline uint16_t trackSport() { return rgb565(24, 10, 12); }
}  // namespace black

// Common (mode-independent)
inline uint16_t warning() { return rgb565(255, 177, 75); }

// Arc gradient colors (toxic neon — lime to emerald)
inline uint16_t arcStart()  { return rgb565(170, 255, 0); }
inline uint16_t arcMid()    { return rgb565(68, 255, 50); }
inline uint16_t arcHigh()   { return rgb565(0, 255, 102); }
inline uint16_t arcEnd()    { return rgb565(0, 204, 136); }

// ── Mode-aware helpers ──
// blackMode() == false → dark palette (default)
// blackMode() == true  → black palette

inline bool isSport(uint8_t drive_mode) { return drive_mode == 2; }

namespace chill {
  inline uint16_t bg()           { return blackMode() ? black::chill::bg()           : dark::chill::bg(); }
  inline uint16_t panel()        { return blackMode() ? black::chill::panel()        : dark::chill::panel(); }
  inline uint16_t line()         { return blackMode() ? black::chill::line()         : dark::chill::line(); }
  inline uint16_t accent()       { return blackMode() ? black::chill::accent()       : dark::chill::accent(); }
  inline uint16_t accentStrong() { return blackMode() ? black::chill::accentStrong() : dark::chill::accentStrong(); }
  inline uint16_t accentDim()    { return blackMode() ? black::chill::accentDim()    : dark::chill::accentDim(); }
  inline uint16_t moodRing()     { return blackMode() ? black::chill::moodRing()     : dark::chill::moodRing(); }
  inline uint16_t accentAlt()    { return blackMode() ? black::chill::accentAlt()    : dark::chill::accentAlt(); }
  inline uint16_t shift()        { return blackMode() ? black::chill::shift()        : dark::chill::shift(); }
}

namespace sport {
  inline uint16_t bg()           { return blackMode() ? black::sport::bg()           : dark::sport::bg(); }
  inline uint16_t panel()        { return blackMode() ? black::sport::panel()        : dark::sport::panel(); }
  inline uint16_t line()         { return blackMode() ? black::sport::line()         : dark::sport::line(); }
  inline uint16_t accent()       { return blackMode() ? black::sport::accent()       : dark::sport::accent(); }
  inline uint16_t accentStrong() { return blackMode() ? black::sport::accentStrong() : dark::sport::accentStrong(); }
  inline uint16_t accentDim()    { return blackMode() ? black::sport::accentDim()    : dark::sport::accentDim(); }
  inline uint16_t moodRing()     { return blackMode() ? black::sport::moodRing()     : dark::sport::moodRing(); }
  inline uint16_t accentAlt()    { return blackMode() ? black::sport::accentAlt()    : dark::sport::accentAlt(); }
  inline uint16_t shift()        { return blackMode() ? black::sport::shift()        : dark::sport::shift(); }
}

inline uint16_t text()  { return blackMode() ? black::text()  : dark::text(); }
inline uint16_t muted() { return blackMode() ? black::muted() : dark::muted(); }

inline uint16_t bg(uint8_t dm)           { return isSport(dm) ? sport::bg()           : chill::bg(); }
inline uint16_t panel(uint8_t dm)        { return isSport(dm) ? sport::panel()        : chill::panel(); }
inline uint16_t line(uint8_t dm)         { return isSport(dm) ? sport::line()         : chill::line(); }
inline uint16_t accent(uint8_t dm)       { return isSport(dm) ? sport::accent()       : chill::accent(); }
inline uint16_t accentStrong(uint8_t dm) { return isSport(dm) ? sport::accentStrong() : chill::accentStrong(); }
inline uint16_t accentDim(uint8_t dm)    { return isSport(dm) ? sport::accentDim()    : chill::accentDim(); }
inline uint16_t moodRing(uint8_t dm)     { return isSport(dm) ? sport::moodRing()     : chill::moodRing(); }
inline uint16_t accentAlt(uint8_t dm)    { return isSport(dm) ? sport::accentAlt()    : chill::accentAlt(); }
inline uint16_t shift(uint8_t dm)        { return isSport(dm) ? sport::shift()        : chill::shift(); }
inline uint16_t track(uint8_t dm)        { return isSport(dm)
    ? (blackMode() ? black::trackSport() : dark::trackSport())
    : (blackMode() ? black::trackChill() : dark::trackChill()); }

}  // namespace theme
