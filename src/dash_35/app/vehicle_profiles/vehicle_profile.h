#pragma once

#include <Arduino.h>

#include "shared_data.h"

namespace app::vehicle_profiles {

constexpr size_t kMaxBluetoothTargets = 4;

enum class FuelSource : uint8_t {
  Standard012F = 0,
  Toyota2129 = 1,
  Auto = 2,
};

struct ObdPidFormula {
  const char *label;
  const char *request;
  const char *formula;
};

struct VehicleProfile {
  const char *profile_id;
  const char *display_name;
  const char *bluetooth_targets[kMaxBluetoothTargets];
  size_t bluetooth_target_count;
  const char *ble_service_uuid;
  const char *ble_tx_uuid;
  const char *ble_rx_uuid;
  float fuel_tank_liters;
  float baseline_efficiency_km_per_l;
  int8_t speed_offset_kph;
  FuelSource fuel_source;
  const char *toyota_fuel_header;
  float toyota_fuel_offset_liters;
  uint8_t fuel_scale_percent;
  uint8_t low_fuel_warning_pct;
  const ObdPidFormula *pid_formulas;
  size_t pid_formula_count;
};

const VehicleProfile &activeProfile();
const char *bluetoothTargetAt(size_t index);
size_t bluetoothTargetCount();
const char *bleServiceUuid();
const char *bleTxUuid();
const char *bleRxUuid();
int16_t applySpeedOffset(int16_t speed_kph);
uint16_t estimateRangeKm(const telemetry::DashboardTelemetry &telemetry);
void refreshTelemetry(telemetry::DashboardTelemetry &telemetry);

}  // namespace app::vehicle_profiles