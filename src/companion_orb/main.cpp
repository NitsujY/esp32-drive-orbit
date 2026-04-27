#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

#include "shared_data.h"
#include "cyber_theme.h"
#include "app/elm327_obd2_client.h"
#include "app/pet_state.h"
#include "app/orb_display.h"
#include "app/wifi_manager.h"
#include "app/weather_client.h"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kStaleThresholdMs = 4000;

orb::Elm327Obd2Client obd2_client;
orb::PetState pet;
orb::OrbDisplay display;
orb::WifiManager wifi_manager;
orb::WeatherClient weather_client;

bool wifi_one_shot_done = false;

uint32_t last_heartbeat_ms = 0;
uint32_t packets_received = 0;

bool debug_view_active = false;
uint32_t last_touch_ms = 0;



bool was_touched = false;

void checkTouch(uint32_t now) {
  if (now - last_touch_ms < 100) return; // 10Hz polling
  last_touch_ms = now;
  
  Wire.beginTransmission(0x15);
  Wire.write(0x02); // Finger count reg
  uint8_t error = Wire.endTransmission(false);
  if (error == 0) {
    Wire.requestFrom((uint8_t)0x15, (uint8_t)1);
    if (Wire.available()) {
      uint8_t fingers = Wire.read();
      
      bool is_touched = (fingers == 1 || fingers == 2);
      if (is_touched && !was_touched) { 
        debug_view_active = !debug_view_active;
        Serial.print("[TOUCH] Debug view toggled: ");
        Serial.println(debug_view_active);
      }
      was_touched = is_touched;
    }
  } else {
    // If I2C hangs, reset the bus
    if (error == 5) {
      Wire.end();
      Wire.begin(4, 5);
      Wire.setTimeOut(100);
    }
  }
}


}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] companion_orb starting (independent OBD2 mode)");

  pet.begin(Serial);
  display.begin(Serial);
  
  // Show splash screen for 2.5 seconds during boot
  delay(2500);

  wifi_manager.begin(Serial);
  weather_client.begin(Serial);

  // Initialize OBD2 client on Serial1
  obd2_client.begin(Serial, 38400);


  Serial.print("[BOOT] MAC: ");
  Serial.println(WiFi.macAddress());


  // Setup I2C for Touch
  Wire.begin(4, 5);
  Wire.setTimeOut(100); // Prevent I2C from hanging on breadboard wire jiggle
  Serial.println("Scanning I2C...");
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at 0x");
      Serial.println(i, HEX);
    }
  }


}

void loop() {

  const uint32_t now = millis();

  checkTouch(now);

  wifi_manager.poll(now);

  telemetry::DashboardTelemetry temp_telemetry;
  weather_client.poll(now, wifi_manager.isConnected(), temp_telemetry);

  if (!wifi_one_shot_done && weather_client.hasFreshWeather() && wifi_manager.isTimeSynced()) {
    wifi_manager.stop("Weather & Time synced, stopping Wi-Fi.");
    wifi_one_shot_done = true;
  }

  // Poll the OBD2 client

  obd2_client.poll(now);

  // If we have fresh data from OBD2, update the display
  if (obd2_client.hasFreshData(now)) {
    // Build a DashboardTelemetry struct from OBD2 data
    telemetry::DashboardTelemetry telemetry;
    telemetry.sequence              = packets_received;
    telemetry.uptime_ms             = now;
    telemetry.speed_kph             = obd2_client.lastSpeed();
    telemetry.rpm                   = obd2_client.lastRpm();
    telemetry.fuel_level_pct        = obd2_client.lastFuelPct();
    telemetry.longitudinal_accel_mg = 0;      // not available from basic OBD2
    telemetry.coolant_temp_c        = 0;      // not in this simplified client
    telemetry.battery_mv            = 0;      // not available
    telemetry.estimated_range_km    = 0;      // not available
    telemetry.nearest_camera_m      = telemetry::kNearestCameraUnknown;
    telemetry.drive_mode            = telemetry::DriveMode::Cruise;
    telemetry.headlights_on         = false;

    telemetry.weather_temp_c        = temp_telemetry.weather_temp_c;
    telemetry.weather_code          = temp_telemetry.weather_code;
    telemetry.wifi_connected        = temp_telemetry.wifi_connected;
    
    // Update pet state based on telemetry
    pet.update(telemetry, now);


    if (debug_view_active) {
      display.renderDebugScreen(obd2_client.statusText(), telemetry, now);
    } else {
      display.render(pet, telemetry, now);
    }


    ++packets_received;
  } else if (!obd2_client.connected()) {
    
    // Show parked screen with weather when OBD is disconnected
    telemetry::DashboardTelemetry telemetry;
    telemetry.sequence              = packets_received;
    telemetry.uptime_ms             = now;
    telemetry.speed_kph             = 0;
    telemetry.rpm                   = 0;
    telemetry.fuel_level_pct        = 0;
    telemetry.longitudinal_accel_mg = 0;
    telemetry.coolant_temp_c        = 0;
    telemetry.battery_mv            = 0;
    telemetry.estimated_range_km    = 0;
    telemetry.nearest_camera_m      = telemetry::kNearestCameraUnknown;
    telemetry.drive_mode            = telemetry::DriveMode::Cruise;
    telemetry.headlights_on         = false;

    telemetry.weather_temp_c        = temp_telemetry.weather_temp_c;
    telemetry.weather_code          = temp_telemetry.weather_code;
    telemetry.wifi_connected        = temp_telemetry.wifi_connected;
    
    pet.update(telemetry, now);

    if (debug_view_active) {
      display.renderDebugScreen(obd2_client.statusText(), telemetry, now);
    } else {
      display.render(pet, telemetry, now);
    }

  }

  // Heartbeat log
  if (now - last_heartbeat_ms >= kHeartbeatIntervalMs) {
    last_heartbeat_ms = now;

    Serial.print("[ORB] Status: ");
    Serial.print(obd2_client.statusText());
    Serial.print(" | Speed: ");
    Serial.print(obd2_client.lastSpeed());
    Serial.print(" km/h, RPM: ");
    Serial.print(obd2_client.lastRpm());
    Serial.print(" | Fuel: ");
    Serial.print(obd2_client.lastFuelPct());
    Serial.print("% | Packets: ");
    Serial.println(packets_received);
  }
}
