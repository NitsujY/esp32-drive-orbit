#pragma once

// Wi-Fi credentials are injected at build time from environment variables.
// Required:
//   DASH_WIFI_SSID
//   DASH_WIFI_PASSWORD
// Optional:
//   DASH_LOCATION_LAT
//   DASH_LOCATION_LON
//   DASH_TIMEZONE_POSIX

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef LOCATION_LAT
#define LOCATION_LAT 35.6762
#endif

#ifndef LOCATION_LON
#define LOCATION_LON 139.6503
#endif

#ifndef TIMEZONE_POSIX
#define TIMEZONE_POSIX "JST-9"
#endif
