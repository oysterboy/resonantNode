#pragma once

#include <Print.h>

/*
FirmwareOta (step 11d, FWOTA; decisions/2026-10-28-ota-firmware-pull-over-http.md)

A board pulls a new firmware image over WiFi: `OTA url=http://<host>:<port>/<file>.bin token=<t>` joins the
network named in include/wifi_secrets.h (local, git-ignored; WIFI_SSID / WIFI_PASSWORD), streams the image into the
other app slot (the default partition table has two, app0 / app1), and reboots into it. The running image is
untouched until the new one is written and checked, so a failed or interrupted download leaves the board on its old
firmware. WiFi is joined without auto-reconnect and without writing the credentials to NVS (review 12.0 M1,
L-OTA4); a failed attempt puts the WiFi mode back as it was, and the Node then restores its ESP-NOW channel.

Token (owner 2026-10-29, review 12.0 M4): the command must carry token= equal to OTA_TOKEN from the same secrets
header. A missing or wrong token answers `OTA result=bad_token` and changes nothing; a build without OTA_TOKEN (or
without the WiFi credentials) refuses every OTA with `no_credentials`. The token keeps a stray ESP32 on the radio
from reflashing the Nodes; it is not encryption (the radio frames and the HTTP image are plain).

The new image starts unconfirmed (the bootloader's rollback, CONFIG_APP_ROLLBACK_ENABLE). Two clocks, both uptime
from esp_timer (not timebase::nowMs, which a wrap-test build offsets; review 12.0 L-OTA1):
- confirm: confirmRunningImage, called from each mode's loop, marks the image valid once uptime reaches
  kConfirmAfterMs. It runs from the loop, so an image whose loop never runs never confirms.
- deadline (owner 2026-10-29, L-OTA2): armRebootDeadline, called first thing in setup(), starts a one-shot esp_timer
  when the running image is still pending verify. If the image has not confirmed by kRebootDeadlineMs of uptime, the
  timer restarts the board and the bootloader rolls back to the previous image: this catches an image that hangs
  without crashing.
An image that crashes or resets before it confirms is rolled back the same way.

Param updates are not this (PAR-013, NET): this changes firmware only.
*/
namespace ota {

enum class Result {
    Ok,             // written; the board reboots (updateFromUrl does not return)
    BadCommand,     // no url=, not http://, or a url too long to keep
    NoCredentials,  // built without WIFI_SSID / WIFI_PASSWORD / OTA_TOKEN (include/wifi_secrets.h)
    BadToken,       // token= missing or not OTA_TOKEN (nothing changed)
    WifiTimeout,    // the network did not answer in time
    DownloadFailed, // HTTP or write error (the old image stays)
    NoUpdate,       // the server had nothing to send
};

constexpr unsigned long kConfirmAfterMs = 20000UL;
constexpr unsigned long kRebootDeadlineMs = 60000UL;

const char* resultName(Result result);

// Parses `OTA url=... token=...` (the line as typed; the verb already matched) and runs the update. Prints
// `OTA start`, progress, and `OTA result=...`. On success it reboots; otherwise it returns with the old image running,
// WiFi disconnected and its mode as before; the caller restores what it needs (the Node its ESP-NOW channel).
Result runCommand(const char* line, Print& out);

// From setup(), before the mode starts: on an unconfirmed image, arms the kRebootDeadlineMs reboot and prints
// `OTA pending_verify confirm_after_ms=<n> reboot_deadline_ms=<n>`. Nothing on a confirmed image.
void armRebootDeadline(Print& out);

// From each mode's loop: once uptime reaches kConfirmAfterMs, an unconfirmed (just updated) image is marked valid,
// the reboot deadline cancelled and `OTA confirmed` printed. Cheap after that.
// ready: the mode is up as it should be (the Node: its radio started, or the radio is switched off).
void confirmRunningImage(Print& out, bool ready = true);

}  // namespace ota
