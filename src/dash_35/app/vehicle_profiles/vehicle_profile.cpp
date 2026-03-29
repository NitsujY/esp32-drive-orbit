#include "vehicle_profile.h"

namespace app::vehicle_profiles {

namespace {

constexpr ObdPidFormula kSientaPidFormulas[] = {
    {"engine_rpm", "010C", "((A * 256) + B) / 4"},
    {"vehicle_speed_kph", "010D", "A"},
    {"coolant_temp_c", "0105", "A - 40"},
    {"fuel_level_pct", "012F", "(A * 100) / 255"},
    {"battery_mv", "ATRV", "volts * 1000"},
};

constexpr VehicleProfile kToyotaSientaProfile = {
    "toyota_sienta",
    "Toyota Sienta",
  {"V-LINK", "IOS-Vlink", "VLINK", "ELM327"},
    4,
  "000018F0-0000-1000-8000-00805F9B34FB",
  "00002AF1-0000-1000-8000-00805F9B34FB",
  "00002AF0-0000-1000-8000-00805F9B34FB",
    42.0f,
    16.5f,
    6,
    FuelSource::Toyota2129,
    "7C0",
    0.0f,
    111,
    18,
    kSientaPidFormulas,
    sizeof(kSientaPidFormulas) / sizeof(kSientaPidFormulas[0]),
};

}  // namespace

const VehicleProfile &activeProfile() {
  return kToyotaSientaProfile;
}

const char *bluetoothTargetAt(size_t index) {
  const VehicleProfile &profile = activeProfile();
  if (index >= profile.bluetooth_target_count) {
    return nullptr;
  }

  return profile.bluetooth_targets[index];
}

size_t bluetoothTargetCount() {
  return activeProfile().bluetooth_target_count;
}

const char *bleServiceUuid() {
  return activeProfile().ble_service_uuid;
}

const char *bleTxUuid() {
  return activeProfile().ble_tx_uuid;
}

const char *bleRxUuid() {
  return activeProfile().ble_rx_uuid;
}

int16_t applySpeedOffset(int16_t speed_kph) {
  // Don't apply offset when OBD reports 0 — car is actually stopped.
  if (speed_kph == 0) {
    return 0;
  }
  const int adjusted = static_cast<int>(speed_kph) + activeProfile().speed_offset_kph;
  return static_cast<int16_t>(max(0, adjusted));
}

uint16_t estimateRangeKm(const telemetry::DashboardTelemetry &telemetry) {
  const VehicleProfile &profile = activeProfile();
  const float fuel_ratio = constrain(telemetry.fuel_level_pct / 100.0f, 0.0f, 1.0f);
  const float liters_remaining = profile.fuel_tank_liters * fuel_ratio;
  const float speed_factor = constrain(1.03f - (telemetry.speed_kph / 320.0f), 0.72f, 1.03f);
  return static_cast<uint16_t>(liters_remaining * profile.baseline_efficiency_km_per_l * speed_factor);
}

void refreshTelemetry(telemetry::DashboardTelemetry &telemetry) {
  telemetry::refreshDerivedTelemetry(telemetry);
  telemetry.estimated_range_km = estimateRangeKm(telemetry);
}

}  // namespace app::vehicle_profiles