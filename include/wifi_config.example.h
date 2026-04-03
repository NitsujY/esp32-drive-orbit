#pragma once

// Wi-Fi credentials are no longer stored in source files.
// Put them in a local .env file or export them before running `pio run -e dash_35`.
// See .env.example for the expected keys.

#define WIFI_SSID "Injected from DASH_WIFI_SSID"
#define WIFI_PASSWORD "Injected from DASH_WIFI_PASSWORD"

// Location for weather/traffic (decimal degrees).
#define LOCATION_LAT 35.6762
#define LOCATION_LON 139.6503

// Timezone (POSIX format).
// Japan: "JST-9"  Taiwan: "CST-8"  US Pacific: "PST8PDT"
#define TIMEZONE_POSIX "JST-9"
