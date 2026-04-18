#include "pet_state.h"

namespace orb {

namespace {

constexpr uint32_t kEventCheckIntervalMs = 500;
constexpr uint32_t kDynamicsIntervalMs = 50;        // fast update for lean
constexpr uint32_t kEnergyTickMs = 2000;
constexpr uint32_t kSleepAfterMs = 30000;
constexpr uint32_t kReactionDurationMs = 1500;
constexpr uint32_t kCelebrateDurationMs = 2500;

constexpr int16_t kHarshAccelThreshold = 18;
constexpr int16_t kHarshBrakeThreshold = 20;
constexpr int16_t kSmoothThreshold = 4;
constexpr int16_t kHighRpmThreshold = 4500;
constexpr int16_t kSportRpmThreshold = 3500;
constexpr uint32_t kHeadbangAfterMs = 4000;          // sustained high RPM
constexpr uint16_t kStreakCelebrateInterval = 20;
constexpr uint8_t kLowFuelThreshold = 15;
constexpr int16_t kColdCoolantThreshold = 50;         // °C
constexpr int16_t kGForceReactionThreshold = 400;     // milli-g
constexpr float kLeanSmoothingFactor = 0.15f;

constexpr uint16_t kXpBaby  = 50;
constexpr uint16_t kXpTeen  = 300;
constexpr uint16_t kXpAdult = 1000;

}  // namespace

void PetState::begin(Print &log) {
  log_ = &log;
  stats_ = PetStats{};
  dynamics_ = DrivingDynamics{};
  last_stage_ = PetStage::Egg;
  log_->println("[PET] Tamagotchi companion hatched!");
}

PetStage PetState::stage() const {
  if (stats_.xp >= kXpAdult) return PetStage::Adult;
  if (stats_.xp >= kXpTeen)  return PetStage::Teen;
  if (stats_.xp >= kXpBaby)  return PetStage::Baby;
  return PetStage::Egg;
}

uint16_t PetState::xpForNextLevel() const {
  switch (stage()) {
    case PetStage::Egg:   return kXpBaby;
    case PetStage::Baby:  return kXpTeen;
    case PetStage::Teen:  return kXpAdult;
    case PetStage::Adult: return kXpAdult + 1000;
  }
  return kXpBaby;
}

uint16_t PetState::xpForCurrentLevel() const {
  switch (stage()) {
    case PetStage::Egg:   return 0;
    case PetStage::Baby:  return kXpBaby;
    case PetStage::Teen:  return kXpTeen;
    case PetStage::Adult: return kXpAdult;
  }
  return 0;
}

float PetState::levelProgress() const {
  const uint16_t base = xpForCurrentLevel();
  const uint16_t cap = xpForNextLevel();
  if (cap <= base) return 1.0f;
  return static_cast<float>(stats_.xp - base) / static_cast<float>(cap - base);
}

uint32_t PetState::reactionAge(uint32_t now_ms) const {
  return now_ms - reaction_start_ms_;
}

bool PetState::isReactionActive(uint32_t now_ms) const {
  if (reaction_ == Reaction::None) return false;
  const uint32_t dur = (reaction_ == Reaction::Celebrate) ? kCelebrateDurationMs : kReactionDurationMs;
  return (now_ms - reaction_start_ms_) < dur;
}

// ─── Main tick ───────────────────────────────────────────────────────────────

void PetState::update(const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  // Trip detection.
  if (t.speed_kph > 5 && !in_trip_) {
    in_trip_ = true;
    had_harsh_event_ = false;
    speed_milestones_hit_ = 0;
    if (!trip_started_) {
      stats_.total_trips++;
      trip_started_ = true;
    }
  }

  // Track if car was moving (for smooth-stop detection).
  if (t.speed_kph > 10) was_moving_ = true;

  // Clear expired reactions.
  if (reaction_ != Reaction::None && !isReactionActive(now_ms)) {
    reaction_ = Reaction::None;
  }

  updateDynamics(t, now_ms);
  checkDrivingEvents(t, now_ms);
  updateEnergy(t, now_ms);
  updateSleep(t, now_ms);

  // Low fuel worry.
  if (t.fuel_level_pct > 0 && t.fuel_level_pct < kLowFuelThreshold &&
      reaction_ != Reaction::Celebrate && reaction_ != Reaction::SpeedCheer) {
    triggerReaction(Reaction::Worried, now_ms);
  }

  prev_speed_ = t.speed_kph;
  prev_rpm_ = t.rpm;
  prev_gear_ = static_cast<uint8_t>(t.gear);
}

// ─── Continuous dynamics (runs fast, ~50 ms) ─────────────────────────────────

void PetState::updateDynamics(const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (now_ms - last_dynamics_ms_ < kDynamicsIntervalMs) return;
  last_dynamics_ms_ = now_ms;

  dynamics_.accel_mg = t.longitudinal_accel_mg;
  dynamics_.gear = static_cast<uint8_t>(t.gear);

  // Smooth lean factor: maps accel_mg to -1..+1 range, smoothed.
  const float target_lean = constrain(
      static_cast<float>(t.longitudinal_accel_mg) / 600.0f, -1.0f, 1.0f);
  dynamics_.lean_factor += (target_lean - dynamics_.lean_factor) * kLeanSmoothingFactor;

  // Speed excitement: 0 at 0 km/h, 1.0 at 160+ km/h.
  dynamics_.speed_excitement = constrain(
      static_cast<float>(t.speed_kph) / 160.0f, 0.0f, 1.0f);

  // Sustained sport-zone tracking.
  if (t.rpm > kSportRpmThreshold) {
    if (high_rpm_start_ms_ == 0) high_rpm_start_ms_ = now_ms;
    dynamics_.in_sport_zone = (now_ms - high_rpm_start_ms_ > 1500);
  } else {
    high_rpm_start_ms_ = 0;
    dynamics_.in_sport_zone = false;
  }
}

// ─── Driving event detection (every 500 ms) ──────────────────────────────────

void PetState::checkDrivingEvents(const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (now_ms - last_event_check_ms_ < kEventCheckIntervalMs) return;
  last_event_check_ms_ = now_ms;

  const int16_t speed_delta = t.speed_kph - prev_speed_;

  // ─ Cold engine reaction (once per boot, before doing anything else).
  if (!cold_start_reacted_ && t.coolant_temp_c > -100 && t.coolant_temp_c < kColdCoolantThreshold) {
    cold_start_reacted_ = true;
    triggerReaction(Reaction::Chilly, now_ms);
    if (log_) { log_->println("[PET] Brr! Engine cold"); }
    return;
  }
  if (t.coolant_temp_c >= kColdCoolantThreshold) {
    cold_start_reacted_ = true;  // don't react once warmed up
  }

  // ─ Harsh acceleration.
  if (speed_delta > kHarshAccelThreshold) {
    addHappiness(-4);
    stats_.streak = 0;
    had_harsh_event_ = true;
    triggerReaction(Reaction::Startled, now_ms);
    if (log_) { log_->println("[PET] Harsh accel!"); }
    return;
  }

  // ─ Harsh braking.
  if (speed_delta < -kHarshBrakeThreshold) {
    addHappiness(-5);
    stats_.streak = 0;
    had_harsh_event_ = true;
    triggerReaction(Reaction::Startled, now_ms);
    if (log_) { log_->println("[PET] Harsh brake!"); }
    return;
  }

  // ─ Strong G-force reaction (not harsh enough to be startled, but noticeable).
  if (abs(t.longitudinal_accel_mg) > kGForceReactionThreshold &&
      reaction_ == Reaction::None) {
    triggerReaction(Reaction::GForce, now_ms);
  }

  // ─ Gear shift detection.
  const uint8_t cur_gear = static_cast<uint8_t>(t.gear);
  if (cur_gear != prev_gear_ && cur_gear > 0 && prev_gear_ > 0 && t.speed_kph > 5) {
    triggerReaction(Reaction::GearShift, now_ms);
    addXp(1);
  }

  // ─ Speed milestones (50, 100, 150 km/h — once per trip each).
  if (in_trip_) {
    if (t.speed_kph >= 50 && !(speed_milestones_hit_ & 0x01)) {
      speed_milestones_hit_ |= 0x01;
      triggerReaction(Reaction::SpeedCheer, now_ms);
      addXp(3);
      addHappiness(2);
      if (log_) { log_->println("[PET] 50 km/h milestone!"); }
    } else if (t.speed_kph >= 100 && !(speed_milestones_hit_ & 0x02)) {
      speed_milestones_hit_ |= 0x02;
      triggerReaction(Reaction::SpeedCheer, now_ms);
      addXp(5);
      addHappiness(3);
      if (log_) { log_->println("[PET] 100 km/h milestone!"); }
    } else if (t.speed_kph >= 150 && !(speed_milestones_hit_ & 0x04)) {
      speed_milestones_hit_ |= 0x04;
      triggerReaction(Reaction::SpeedCheer, now_ms);
      addXp(8);
      addHappiness(5);
      if (log_) { log_->println("[PET] 150 km/h milestone!"); }
    }
  }

  // ─ Sustained high RPM → headbang.
  if (t.rpm > kHighRpmThreshold && high_rpm_start_ms_ > 0 &&
      (now_ms - high_rpm_start_ms_ > kHeadbangAfterMs) &&
      reaction_ == Reaction::None) {
    triggerReaction(Reaction::Headbang, now_ms);
    addXp(2);
  }

  // ─ High RPM burst (crossing threshold).
  if (t.rpm > kHighRpmThreshold && prev_rpm_ <= kHighRpmThreshold) {
    triggerReaction(Reaction::Stars, now_ms);
    addXp(2);
    return;
  }

  // ─ Smooth stop detection: was moving, now stopped, no harsh events.
  if (was_moving_ && t.speed_kph == 0 && prev_speed_ <= 5 && !had_harsh_event_) {
    triggerReaction(Reaction::SmoothStop, now_ms);
    addXp(3);
    addHappiness(4);
    was_moving_ = false;
    if (log_) { log_->println("[PET] Smooth stop! +happiness"); }
    return;
  }
  if (t.speed_kph == 0) {
    was_moving_ = false;
    had_harsh_event_ = false;  // reset for next departure
  }

  // ─ Smooth driving window.
  if (t.speed_kph > 5 && abs(speed_delta) <= kSmoothThreshold) {
    stats_.streak++;
    addHappiness(1);
    addXp(1);

    if (stats_.streak > stats_.best_streak) {
      stats_.best_streak = stats_.streak;
    }

    if (stats_.streak > 0 && (stats_.streak % kStreakCelebrateInterval) == 0) {
      triggerReaction(Reaction::Hearts, now_ms);
      addXp(5);
      addHappiness(3);
      if (log_) {
        log_->print("[PET] Smooth streak ");
        log_->print(stats_.streak);
        log_->println("!");
      }
    }
  }
}

void PetState::updateEnergy(const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (now_ms - last_energy_update_ms_ < kEnergyTickMs) return;
  last_energy_update_ms_ = now_ms;

  if (t.speed_kph > 0) {
    if (stats_.energy > 0) stats_.energy--;
    if (t.drive_mode == telemetry::DriveMode::Sport && stats_.energy > 0)
      stats_.energy--;
  } else {
    if (stats_.energy < 100) stats_.energy++;
    if (sleeping_ && stats_.energy < 100) stats_.energy++;
  }
}

void PetState::updateSleep(const telemetry::DashboardTelemetry &t, uint32_t now_ms) {
  if (t.speed_kph > 0 || t.rpm > 100) {
    last_motion_ms_ = now_ms;
    if (sleeping_) {
      sleeping_ = false;
      triggerReaction(Reaction::Stars, now_ms);
      if (log_) { log_->println("[PET] Woke up!"); }
    }
  } else if (!sleeping_ && last_motion_ms_ > 0 &&
             (now_ms - last_motion_ms_ > kSleepAfterMs)) {
    sleeping_ = true;
    in_trip_ = false;
    trip_started_ = false;
    triggerReaction(Reaction::Sleepy, now_ms);
    if (log_) { log_->println("[PET] Falling asleep..."); }
  }
}

void PetState::triggerReaction(Reaction r, uint32_t now_ms) {
  if (reaction_ == Reaction::Celebrate && isReactionActive(now_ms)) return;
  reaction_ = r;
  reaction_start_ms_ = now_ms;
}

void PetState::addXp(uint16_t amount) {
  const PetStage old_stage = stage();
  stats_.xp = min(static_cast<uint16_t>(stats_.xp + amount), static_cast<uint16_t>(65535U));

  const PetStage new_stage = stage();
  if (new_stage != old_stage) {
    reaction_ = Reaction::Celebrate;
    reaction_start_ms_ = millis();
    addHappiness(20);
    if (log_) {
      log_->print("[PET] EVOLVED to stage ");
      log_->println(static_cast<uint8_t>(new_stage));
    }
    last_stage_ = new_stage;
  }
}

void PetState::addHappiness(int8_t delta) {
  int16_t val = static_cast<int16_t>(stats_.happiness) + delta;
  stats_.happiness = static_cast<uint8_t>(constrain(val, 0, 100));
}

}  // namespace orb
