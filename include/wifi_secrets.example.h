#pragma once
// Copy to include/wifi_secrets.h (git ignores it) and fill in the network the OTA firmware update joins.
// Without that file the build has no WiFi credentials and OTA reports "no_credentials".
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
