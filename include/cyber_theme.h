#pragma once

#include <Arduino.h>

namespace theme {

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ── Cyber palette (shared between dash_35 and companion_orb) ──

// Chill mode: deep blue-black with cyan/teal accents
namespace chill {
  inline uint16_t bg()           { return rgb565(4, 8, 18); }
  inline uint16_t panel()        { return rgb565(8, 16, 32); }
  inline uint16_t line()         { return rgb565(20, 50, 70); }
  inline uint16_t accent()       { return rgb565(0, 255, 200); }     // cyan-mint
  inline uint16_t accentStrong() { return rgb565(180, 255, 240); }
  inline uint16_t accentDim()    { return rgb565(0, 120, 100); }
  inline uint16_t moodRing()     { return rgb565(0, 180, 160); }     // teal glow
}  // namespace chill

// Sport mode: dark red-black with hot pink/orange accents
namespace sport {
  inline uint16_t bg()           { return rgb565(18, 4, 8); }
  inline uint16_t panel()        { return rgb565(32, 8, 14); }
  inline uint16_t line()         { return rgb565(70, 20, 30); }
  inline uint16_t accent()       { return rgb565(255, 50, 120); }    // hot pink
  inline uint16_t accentStrong() { return rgb565(255, 180, 210); }
  inline uint16_t accentDim()    { return rgb565(120, 20, 50); }
  inline uint16_t moodRing()     { return rgb565(255, 40, 80); }     // red-pink glow
}  // namespace sport

// Common
inline uint16_t text()    { return rgb565(230, 245, 255); }
inline uint16_t muted()   { return rgb565(100, 130, 150); }
inline uint16_t warning() { return rgb565(255, 200, 60); }

// Arc gradient colors (neon sweep)
inline uint16_t arcStart()  { return rgb565(255, 0, 200); }   // magenta
inline uint16_t arcMid()    { return rgb565(120, 0, 255); }   // purple
inline uint16_t arcHigh()   { return rgb565(0, 120, 255); }   // blue
inline uint16_t arcEnd()    { return rgb565(0, 255, 220); }   // cyan

// Track (inactive arc)
inline uint16_t trackChill() { return rgb565(20, 40, 55); }
inline uint16_t trackSport() { return rgb565(55, 20, 30); }

// Mode-aware helpers
inline bool isSport(uint8_t drive_mode) { return drive_mode == 2; }

inline uint16_t bg(uint8_t dm)           { return isSport(dm) ? sport::bg()           : chill::bg(); }
inline uint16_t panel(uint8_t dm)        { return isSport(dm) ? sport::panel()        : chill::panel(); }
inline uint16_t line(uint8_t dm)         { return isSport(dm) ? sport::line()          : chill::line(); }
inline uint16_t accent(uint8_t dm)       { return isSport(dm) ? sport::accent()       : chill::accent(); }
inline uint16_t accentStrong(uint8_t dm) { return isSport(dm) ? sport::accentStrong() : chill::accentStrong(); }
inline uint16_t accentDim(uint8_t dm)    { return isSport(dm) ? sport::accentDim()    : chill::accentDim(); }
inline uint16_t moodRing(uint8_t dm)     { return isSport(dm) ? sport::moodRing()     : chill::moodRing(); }
inline uint16_t track(uint8_t dm)        { return isSport(dm) ? trackSport()          : trackChill(); }

}  // namespace theme
