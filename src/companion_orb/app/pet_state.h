#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace orb {

// Evolution stages — visual complexity increases with each.
enum class PetStage : uint8_t {
  Egg   = 0,  // XP 0–49:   pulsing oval, no face
  Baby  = 1,  // XP 50–299: small round body, dot eyes, line mouth
  Teen  = 2,  // XP 300–999: larger body, expressive eyes, arms
  Adult = 3,  // XP 1000+:  full-size, antenna/ears, detailed face
};

// Momentary reactions triggered by driving events.
enum class Reaction : uint8_t {
  None        = 0,
  Startled    = 1,   // harsh accel/brake — eyes wide, body jumps
  Hearts      = 2,   // smooth streak — floating hearts
  Stars       = 3,   // high RPM burst — spinning stars around head
  Sleepy      = 4,   // idle too long — yawning, zzz
  Celebrate   = 5,   // level-up or long streak — confetti burst
  Worried     = 6,   // low fuel — sweat drop
  GForce      = 7,   // strong longitudinal G — pet leans hard
  GearShift   = 8,   // gear change detected — pet bounces
  SpeedCheer  = 9,   // hit a speed milestone (50/100/150) — cheering
  Chilly      = 10,  // cold engine (coolant < 50°C) — shivering
  SmoothStop  = 11,  // came to a stop without any harsh event — thumbs up
  Headbang    = 12,  // sustained high RPM in sport — head-bobbing to "music"
};

// Persistent pet state (all session-local, not NVS-backed yet).
struct PetStats {
  uint8_t happiness = 60;          // 0–100
  uint8_t energy    = 100;         // 0–100
  uint16_t xp       = 0;          // 0–65535
  uint16_t streak   = 0;          // consecutive smooth windows
  uint16_t total_trips = 0;       // drives where speed went > 0
  uint16_t best_streak = 0;       // longest smooth streak ever
};

// Continuous driving dynamics exposed to the display layer.
struct DrivingDynamics {
  int16_t accel_mg = 0;           // raw longitudinal G (forward/back lean)
  float lean_factor = 0.0f;       // -1.0 (hard brake) to +1.0 (hard accel)
  float speed_excitement = 0.0f;  // 0.0–1.0 based on current speed vs max
  bool in_sport_zone = false;     // RPM > 3500 sustained
  uint8_t gear = 0;               // current gear number for display
};

class PetState {
 public:
  void begin(Print &log);

  // Call every telemetry tick (~100 ms).
  void update(const telemetry::DashboardTelemetry &telemetry, uint32_t now_ms);

  // Accessors for the display.
  PetStage stage() const;
  uint8_t happiness() const { return stats_.happiness; }
  uint8_t energy() const { return stats_.energy; }
  uint16_t xp() const { return stats_.xp; }
  uint16_t xpForNextLevel() const;
  uint16_t xpForCurrentLevel() const;
  float levelProgress() const;  // 0.0–1.0 within current stage

  Reaction currentReaction() const { return reaction_; }
  uint32_t reactionAge(uint32_t now_ms) const;
  bool isReactionActive(uint32_t now_ms) const;

  const DrivingDynamics &dynamics() const { return dynamics_; }
  bool isSleeping() const { return sleeping_; }
  bool isInTrip() const { return in_trip_; }
  uint16_t streak() const { return stats_.streak; }

 private:
  void checkDrivingEvents(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void updateDynamics(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void updateEnergy(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void updateSleep(const telemetry::DashboardTelemetry &t, uint32_t now_ms);
  void triggerReaction(Reaction r, uint32_t now_ms);
  void addXp(uint16_t amount);
  void addHappiness(int8_t delta);

  Print *log_ = nullptr;
  PetStats stats_{};
  DrivingDynamics dynamics_{};

  // Driving event detection.
  int16_t prev_speed_ = 0;
  int16_t prev_rpm_ = 0;
  uint8_t prev_gear_ = 0;
  uint32_t last_event_check_ms_ = 0;
  uint32_t last_energy_update_ms_ = 0;
  uint32_t last_dynamics_ms_ = 0;

  // Speed milestones: bitfield for 50/100/150 cheered this trip.
  uint8_t speed_milestones_hit_ = 0;

  // Smooth-stop tracking.
  bool had_harsh_event_ = false;
  bool was_moving_ = false;

  // Sustained high-RPM tracking for headbang.
  uint32_t high_rpm_start_ms_ = 0;

  // Cold-engine reaction (fires once per boot).
  bool cold_start_reacted_ = false;

  // Reaction state.
  Reaction reaction_ = Reaction::None;
  uint32_t reaction_start_ms_ = 0;

  // Sleep / idle.
  uint32_t last_motion_ms_ = 0;
  bool sleeping_ = false;
  bool in_trip_ = false;
  bool trip_started_ = false;

  // Level-up tracking.
  PetStage last_stage_ = PetStage::Egg;
};

}  // namespace orb
