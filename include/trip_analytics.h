#pragma once

#include <Arduino.h>

#include <math.h>
#include <stddef.h>

namespace app::trip_analytics {

constexpr float kStoichAfr = 14.7f;
constexpr float kGasolineDensityGramsPerLiter = 717.0f;
constexpr float kLitersPerGallon = 3.785411784f;

struct FuelConsumptionMetrics {
  float maf_gps;
  float liters_per_hour;
  float liters_per_100km;
  float gallons_per_hour;
};

struct MockAccelerationSample {
  uint8_t step_index;
  float speed_kph;
  float maf_gps;
  FuelConsumptionMetrics consumption;
};

inline uint8_t clampScore(int value) {
  return static_cast<uint8_t>(constrain(value, 0, 100));
}

inline FuelConsumptionMetrics calculateFuelConsumption(float maf_gps, float speed_kph) {
  FuelConsumptionMetrics metrics{};
  metrics.maf_gps = max(0.0f, maf_gps);

  const float fuel_grams_per_second = metrics.maf_gps / kStoichAfr;
  metrics.liters_per_hour = (fuel_grams_per_second * 3600.0f) / kGasolineDensityGramsPerLiter;
  metrics.gallons_per_hour = metrics.liters_per_hour / kLitersPerGallon;

  if (speed_kph > 0.5f) {
    metrics.liters_per_100km = (metrics.liters_per_hour / speed_kph) * 100.0f;
  } else {
    metrics.liters_per_100km = 0.0f;
  }

  return metrics;
}

inline float estimateMafGps(int16_t rpm, int16_t speed_kph, int16_t longitudinal_accel_mg = 0) {
  if (rpm <= 0 && speed_kph <= 0) {
    return 0.0f;
  }

  const float idle_component = speed_kph <= 1 ? 2.3f : 0.9f;
  const float rpm_component = max(0, rpm - 650) * 0.0022f;
  const float speed_component = max(0.0f, static_cast<float>(speed_kph)) * 0.031f;
  const float accel_component = max(0.0f, static_cast<float>(longitudinal_accel_mg)) * 0.0052f;
  return constrain(idle_component + rpm_component + speed_component + accel_component, 0.0f,
                   72.0f);
}

inline uint8_t scoreFuelSaving(float average_l_per_100km,
                               float target_l_per_100km,
                               uint8_t harsh_acceleration_count) {
  if (average_l_per_100km <= 0.0f || target_l_per_100km <= 0.0f) {
    return 100;
  }

  int score = 100 - static_cast<int>(lroundf(max(0.0f, average_l_per_100km - target_l_per_100km) * 9.0f)) -
              static_cast<int>(harsh_acceleration_count) * 5;
  if (average_l_per_100km <= target_l_per_100km * 0.92f) {
    score += 4;
  }
  return clampScore(score);
}

inline uint8_t scoreComfort(int16_t longitudinal_accel_mg,
                            uint8_t harsh_acceleration_count,
                            uint8_t harsh_braking_count) {
  const int accel_penalty = max(0, abs(longitudinal_accel_mg) - 140) / 10;
  const int score = 100 - accel_penalty - static_cast<int>(harsh_acceleration_count) * 6 -
                    static_cast<int>(harsh_braking_count) * 8;
  return clampScore(score);
}

inline uint8_t scoreFlow(int16_t speed_delta_kph, int16_t speed_kph) {
  const int score = 96 - abs(speed_delta_kph) * 9 - max(0, speed_kph - 105) / 2;
  return clampScore(score);
}

inline uint8_t scoreTripOverall(uint8_t fuel_saving_score,
                                uint8_t comfort_score,
                                uint8_t flow_score) {
  const int weighted_score = static_cast<int>(fuel_saving_score) * 4 +
                             static_cast<int>(comfort_score) * 4 +
                             static_cast<int>(flow_score) * 2;
  return clampScore((weighted_score + 5) / 10);
}

inline const char *scoreBand(uint8_t score) {
  if (score >= 88) {
    return "excellent";
  }
  if (score >= 72) {
    return "steady";
  }
  if (score >= 56) {
    return "watch";
  }
  return "pushy";
}

inline size_t fillMockAccelerationToSixtyCase(MockAccelerationSample *samples,
                                              size_t sample_capacity) {
  static constexpr float kSpeedsKph[] = {0.0f, 8.0f, 16.0f, 24.0f, 32.0f, 40.0f, 48.0f, 54.0f,
                                         60.0f};
  static constexpr float kMafGps[] = {2.6f, 4.1f, 5.9f, 7.8f, 10.4f, 13.7f, 17.2f, 20.1f, 22.8f};
  const size_t sample_count = min(sample_capacity, sizeof(kSpeedsKph) / sizeof(kSpeedsKph[0]));

  for (size_t index = 0; index < sample_count; ++index) {
    samples[index].step_index = static_cast<uint8_t>(index);
    samples[index].speed_kph = kSpeedsKph[index];
    samples[index].maf_gps = kMafGps[index];
    samples[index].consumption = calculateFuelConsumption(kMafGps[index], kSpeedsKph[index]);
  }

  return sample_count;
}

}  // namespace app::trip_analytics