#pragma once

#include <Print.h>

/*
FirmwareOta (step 11d, FWOTA; decisions/2026-10-28-ota-firmware-pull-over-http.md)

A board pulls a new firmware image over WiFi: `OTA url=http://<host>:<port>/<file>.bin` joins the network named in
include/wifi_secrets.h (local, git-ignored; WIFI_SSID / WIFI_PASSWORD), streams the image into the other app slot
(the default partition table has two, app0 / app1), and reboots into it. The running image is untouched until the
new one is written and checked, so a failed or interrupted download leaves the board on its old firmware.

The new image starts unconfirmed (the bootloader's rollback, CONFIG_APP_ROLLBACK_ENABLE): it is marked valid only
after it has run kConfirmAfterMs (confirmRunningImage from each mode's loop). A new image that crashes or resets
before that is rolled back by the bootloader to the previous one on the next boot.

Param updates are not this (PAR-013, NET): this changes firmware only.
*/
namespace ota {

enum class Result {
    Ok,             // written; the board reboots (updateFromUrl does not return)
    BadCommand,     // no url=, or not http://
    NoCredentials,  // built without include/wifi_secrets.h
    WifiTimeout,    // the network did not answer in time
    DownloadFailed, // HTTP or write error (the old image stays)
    NoUpdate,       // the server had nothing to send
};

constexpr unsigned long kConfirmAfterMs = 20000UL;

const char* resultName(Result result);

// Parses `OTA url=...` (the line as typed; the verb already matched) and runs the update. Prints
// `OTA start`, progress, and `OTA result=...`. On success it reboots; otherwise it returns with the old image running
// and WiFi stopped; the caller restores what it needs (the Node its ESP-NOW channel).
Result runCommand(const char* line, Print& out);

// From each mode's loop: once the image has run kConfirmAfterMs, an unconfirmed (just updated) image is marked
// valid and `OTA confirmed` printed. Cheap after that.
void confirmRunningImage(unsigned long nowMs, Print& out);

}  // namespace ota
