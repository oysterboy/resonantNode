#pragma once
// Copy to include/wifi_secrets.h (git ignores it) and fill in the network the OTA firmware update joins.
// Without that file the build has no WiFi credentials and OTA reports "no_credentials".
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
// The shared OTA token (review 12.0 M4): `OTA url=... token=<this>`; a missing or wrong token is refused
// (`OTA result=bad_token`), a build without it refuses every OTA. Pick your own; tools/bench/otaserve.py prints it.
#define OTA_TOKEN "replace-with-16-hex"
