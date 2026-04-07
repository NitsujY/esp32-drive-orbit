#include "simulation_controller.h"

#include <math.h>

namespace app {

namespace {

constexpr uint8_t kBootButtonPin = 0;
constexpr uint32_t kButtonSampleIntervalMs = 35;
constexpr uint32_t kToggleCooldownMs = 500;
constexpr uint32_t kSimulationIntervalMs = 125;
constexpr float kSimulationCycleSeconds = 58.0f;

template <typename T>
T clampValue(T value, T min_value, T max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

bool readButtonDown() {
  return digitalRead(kBootButtonPin) == LOW;
}

}  // namespace

void SimulationController::begin(Print &log_output) {
  log_output_ = &log_output;
  pinMode(kBootButtonPin, INPUT_PULLUP);
  if (log_output_ != nullptr) {
    log_output_->println("[SIM] BOOT button ready. Press after startup to toggle demo telemetry.");
  }
}

void SimulationController::poll(uint32_t now_ms) {
  if (now_ms - last_button_sample_ms_ < kButtonSampleIntervalMs) {
    return;
  }

  last_button_sample_ms_ = now_ms;
  const bool button_down = readButtonDown();
  const bool cooldown_elapsed = last_toggle_ms_ == 0 || now_ms - last_toggle_ms_ > kToggleCooldownMs;
  if (button_down && !button_was_down_ && cooldown_elapsed) {
    enabled_ = !enabled_;
    last_toggle_ms_ = now_ms;
    if (enabled_) {
      resetState();
    }
    if (log_output_ != nullptr) {
      log_output_->print("[SIM] Demo telemetry ");
      log_output_->println(enabled_ ? "enabled" : "disabled");
    }
  }

  button_was_down_ = button_down;
}

bool SimulationController::enabled() const {
  return enabled_;
}

bool SimulationController::apply(telemetry::CarTelemetry &telemetry,
                                 uint32_t now_ms,
                                 bool wifi_connected) {
  if (!enabled_) {
    return false;
  }

  bool advanced = false;
  if (last_tick_ms_ == 0 || now_ms - last_tick_ms_ >= kSimulationIntervalMs) {
    const uint32_t delta_ms = last_tick_ms_ == 0
                                  ? kSimulationIntervalMs
                                  : clampValue<uint32_t>(now_ms - last_tick_ms_, 16U, 250U);
    last_tick_ms_ = now_ms;
    advanced = true;

    sequence_ += 1;
    phase_s_ += static_cast<float>(delta_ms) / 1000.0f;
    uptime_ms_ += delta_ms;

    const float phase_seconds = fmodf(phase_s_, kSimulationCycleSeconds);
    const float sweep = (sinf(phase_s_ * 0.28f) + 1.0f) * 0.5f;
    const int16_t speed_kph = static_cast<int16_t>(lroundf(sampleSpeed(phase_seconds)));
    const int16_t speed_delta = sequence_ == 1 ? 0 : static_cast<int16_t>(speed_kph - last_speed_kph_);
    const int16_t accel_mg = delta_ms == 0
                                 ? 0
                                 : clampValue<int16_t>(
                                       static_cast<int16_t>(lroundf(
                                           (static_cast<float>(speed_delta) /
                                            (static_cast<float>(delta_ms) / 1000.0f)) *
                                           18.0f)),
                                       static_cast<int16_t>(-220), static_cast<int16_t>(220));
    const int16_t positive_load = accel_mg > 0 ? accel_mg : 0;
    const int16_t rpm = clampValue<int16_t>(
        static_cast<int16_t>(lroundf(780.0f + static_cast<float>(speed_kph) * 17.0f +
                                     static_cast<float>(positive_load) * 1.15f +
                                     sinf(phase_s_ * 1.4f) * 54.0f)),
        static_cast<int16_t>(760), static_cast<int16_t>(3400));
    const int16_t coolant_temp_c = clampValue<int16_t>(
        static_cast<int16_t>(lroundf(80.0f + sweep * 10.0f)), static_cast<int16_t>(78),
        static_cast<int16_t>(94));
    const int8_t weather_temp_c = clampValue<int>(
        static_cast<int>(lroundf(22.0f + sinf(phase_s_ * 0.12f) * 7.0f)), 14, 33);
    const uint16_t battery_mv = clampValue<int>(
        static_cast<int>(lroundf(12420.0f + cosf(phase_s_ * 1.3f) * 180.0f)), 12100, 12780);
    const uint8_t fuel_level_pct = clampValue<int>(
        static_cast<int>(lroundf(82.0f - fmodf(static_cast<float>(sequence_) / 1800.0f, 58.0f))),
        24, 82);
    const bool headlights_on = sinf(phase_s_ * 0.16f) > 0.45f;
    constexpr uint8_t kWeatherCodes[] = {0, 1, 2, 3, 61, 95};
    const uint8_t weather_code = kWeatherCodes[(sequence_ / 90U) % 6U];

    snapshot_ = telemetry::makeInitialCarTelemetry();
    snapshot_.sequence = sequence_;
    snapshot_.uptime_ms = uptime_ms_;
    snapshot_.speed_kph = speed_kph;
    snapshot_.rpm = rpm;
    snapshot_.longitudinal_accel_mg = accel_mg;
    snapshot_.coolant_temp_c = coolant_temp_c;
    snapshot_.weather_temp_c = weather_temp_c;
    snapshot_.weather_code = weather_code;
    snapshot_.battery_mv = battery_mv;
    snapshot_.fuel_level_pct = fuel_level_pct;
    snapshot_.headlights_on = headlights_on;
    telemetry::refreshDerivedTelemetry(snapshot_);
    last_speed_kph_ = speed_kph;
  }

  snapshot_.wifi_connected = wifi_connected;
  snapshot_.obd_connected = true;
  snapshot_.telemetry_fresh = true;
  telemetry = snapshot_;
  return advanced;
}

void SimulationController::resetState() {
  sequence_ = 0;
  phase_s_ = 0.0f;
  uptime_ms_ = 0;
  last_tick_ms_ = 0;
  last_speed_kph_ = 0;
  snapshot_ = telemetry::makeInitialCarTelemetry();
}

float SimulationController::easeInOutSine(float value) {
  return 0.5f - (cosf(static_cast<float>(M_PI) * value) * 0.5f);
}

float SimulationController::sampleSpeed(float phase_seconds) {
  if (phase_seconds < 3.0f) {
    return 0.0f;
  }

  if (phase_seconds < 8.0f) {
    return easeInOutSine((phase_seconds - 3.0f) / 5.0f) * 26.0f;
  }

  if (phase_seconds < 14.0f) {
    const float local_phase = phase_seconds - 8.0f;
    return 26.0f + sinf(local_phase * 0.72f) * 1.4f;
  }

  if (phase_seconds < 20.0f) {
    return 26.0f + easeInOutSine((phase_seconds - 14.0f) / 6.0f) * 44.0f;
  }

  if (phase_seconds < 25.0f) {
    const float local_phase = phase_seconds - 20.0f;
    return 70.0f + sinf(local_phase * 0.64f) * 1.6f;
  }

  if (phase_seconds < 31.0f) {
    return 70.0f - easeInOutSine((phase_seconds - 25.0f) / 6.0f) * 58.0f;
  }

  if (phase_seconds < 35.0f) {
    return 12.0f - easeInOutSine((phase_seconds - 31.0f) / 4.0f) * 12.0f;
  }

  if (phase_seconds < 39.0f) {
    return 0.0f;
  }

  if (phase_seconds < 44.0f) {
    return easeInOutSine((phase_seconds - 39.0f) / 5.0f) * 24.0f;
  }

  if (phase_seconds < 48.0f) {
    const float local_phase = phase_seconds - 44.0f;
    return 24.0f + sinf(local_phase * 0.9f) * 1.1f;
  }

  if (phase_seconds < 54.0f) {
    return 24.0f - easeInOutSine((phase_seconds - 48.0f) / 6.0f) * 24.0f;
  }

  return 0.0f;
}

}  // namespace app