#include "FirmwareOta.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <string.h>

#include "TimingUtils.h"

#if __has_include(<wifi_secrets.h>)
#include <wifi_secrets.h>
#endif

// The core marks every image valid at startup unless this says "later" (esp32-hal-misc.c). Later: after
// kConfirmAfterMs of running, so an image that crashes early is rolled back by the bootloader.
extern "C" bool verifyRollbackLater() {
    return true;
}

namespace ota {

namespace {
constexpr unsigned long kWifiTimeoutMs = 20000UL;
constexpr size_t kUrlCapacity = 160;
constexpr size_t kTokenCapacity = 65;
bool gConfirmChecked = false;
esp_timer_handle_t gDeadlineTimer = nullptr;

// Uptime since boot (esp_timer), untouched by the wrap-test offset of timebase::nowMs (review 12.0 L-OTA1).
unsigned long uptimeMs() {
    return static_cast<unsigned long>(esp_timer_get_time() / 1000);
}

bool runningImagePendingVerify(const esp_partition_t** runningOut) {
    esp_ota_img_states_t state;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (runningOut != nullptr) {
        *runningOut = running;
    }
    return running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

enum class Field { Missing, TooLong, Ok };

// The value of `key` (a word of its own: at the line start or after a space) up to the end of the line or the next
// space, into `out`. TooLong when it does not fit (nothing is cut silently; review 12.0 L-OTA3).
Field parseField(const char* line, const char* key, char* out, size_t capacity) {
    const size_t keyLen = strlen(key);
    const char* p = line;
    while ((p = strstr(p, key)) != nullptr) {
        if (p == line || p[-1] == ' ') {
            break;
        }
        p += keyLen;
    }
    if (p == nullptr) {
        return Field::Missing;
    }
    p += keyLen;
    size_t n = 0;
    while (p[n] != '\0' && p[n] != ' ') {
        if (n + 1 >= capacity) {
            out[0] = '\0';
            return Field::TooLong;
        }
        out[n] = p[n];
        ++n;
    }
    out[n] = '\0';
    return n > 0 ? Field::Ok : Field::Missing;
}

#if defined(OTA_TOKEN)
// Same length and same bytes; the loop does not stop at the first difference.
bool tokenMatches(const char* given) {
    const char* expected = OTA_TOKEN;
    const size_t len = strlen(expected);
    if (len == 0 || strlen(given) != len) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= static_cast<unsigned char>(given[i] ^ expected[i]);
    }
    return diff == 0;
}
#endif

void deadlineExpired(void*) {
    // Still unconfirmed at the deadline: restart; the bootloader rolls back to the previous image.
    esp_restart();
}
}  // namespace

const char* resultName(Result result) {
    switch (result) {
        case Result::Ok:
            return "ok";
        case Result::BadCommand:
            return "bad_command";
        case Result::NoCredentials:
            return "no_credentials";
        case Result::BadToken:
            return "bad_token";
        case Result::WifiTimeout:
            return "wifi_timeout";
        case Result::DownloadFailed:
            return "download_failed";
        case Result::NoUpdate:
            return "no_update";
    }
    return "unknown";
}

Result runCommand(const char* line, Print& out) {
    char url[kUrlCapacity];
    const Field urlField = parseField(line, "url=", url, sizeof(url));
    if (urlField == Field::TooLong) {
        out.print("OTA result=bad_command (url longer than ");
        out.print(static_cast<unsigned>(kUrlCapacity - 1));
        out.println(" characters)");
        return Result::BadCommand;
    }
    if (urlField != Field::Ok || strlen(url) <= 7 || strncmp(url, "http://", 7) != 0) {
        out.println("OTA result=bad_command (use OTA url=http://<host>:<port>/<file>.bin token=<t>)");
        return Result::BadCommand;
    }
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(OTA_TOKEN)
    out.println("OTA result=no_credentials (include/wifi_secrets.h or its OTA_TOKEN missing at build time)");
    return Result::NoCredentials;
#else
    char token[kTokenCapacity];
    if (parseField(line, "token=", token, sizeof(token)) != Field::Ok || !tokenMatches(token)) {
        out.println("OTA result=bad_token");
        return Result::BadToken;
    }
    out.print("OTA start url=");
    out.println(url);
    // No auto-reconnect: after a failure the STA must not rescan and hop channels under ESP-NOW (review 12.0 M1).
    // Not persistent: the credentials are not written to the board's NVS (L-OTA4).
    const wifi_mode_t modeBefore = WiFi.getMode();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long startMs = uptimeMs();
    Result failure = Result::WifiTimeout;
    while (WiFi.status() != WL_CONNECTED) {
        if (timing::elapsedSince(uptimeMs(), startMs, kWifiTimeoutMs)) {
            out.println("OTA result=wifi_timeout");
            break;
        }
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        out.print("OTA wifi ip=");
        out.print(WiFi.localIP());
        out.print(" rssi=");
        out.println(WiFi.RSSI());

        int lastTenth = -1;
        httpUpdate.onProgress([&out, &lastTenth](int done, int total) {
            const int tenth = total > 0 ? (done * 10) / total : 0;
            if (tenth != lastTenth) {
                lastTenth = tenth;
                out.print("OTA progress pct=");
                out.println(tenth * 10);
            }
        });
        httpUpdate.rebootOnUpdate(false);
        WiFiClient client;
        const t_httpUpdate_return result = httpUpdate.update(client, url);
        if (result == HTTP_UPDATE_OK) {
            out.println("OTA result=ok rebooting (the new image confirms itself after 20 s)");
            out.flush();
            delay(300);
            ESP.restart();
            return Result::Ok;
        }
        const bool none = result == HTTP_UPDATE_NO_UPDATES;
        out.print("OTA result=");
        out.print(none ? "no_update" : "download_failed");
        out.print(" error=");
        out.print(httpUpdate.getLastError());
        out.print(" detail=");
        out.println(httpUpdate.getLastErrorString());
        failure = none ? Result::NoUpdate : Result::DownloadFailed;
    }
    WiFi.disconnect();
    // The mode as before the join: the Node keeps STA for ESP-NOW (then restores its channel), the Analyzer and the
    // Emitter go back to WiFi off.
    if (WiFi.getMode() != modeBefore) {
        WiFi.mode(modeBefore);
    }
    return failure;
#endif
}

void armRebootDeadline(Print& out) {
    if (gDeadlineTimer != nullptr || !runningImagePendingVerify(nullptr)) {
        return;
    }
    esp_timer_create_args_t args = {};
    args.callback = &deadlineExpired;
    args.name = "ota_deadline";
    if (esp_timer_create(&args, &gDeadlineTimer) != ESP_OK) {
        gDeadlineTimer = nullptr;
        out.println("OTA pending_verify reboot_deadline=unavailable");
        return;
    }
    const unsigned long now = uptimeMs();
    const unsigned long remainingMs = now < kRebootDeadlineMs ? kRebootDeadlineMs - now : 1UL;
    esp_timer_start_once(gDeadlineTimer, static_cast<uint64_t>(remainingMs) * 1000ULL);
    out.print("OTA pending_verify confirm_after_ms=");
    out.print(kConfirmAfterMs);
    out.print(" reboot_deadline_ms=");
    out.println(kRebootDeadlineMs);
}

void confirmRunningImage(Print& out, bool ready) {
    const unsigned long nowMs = uptimeMs();
#if defined(OTA_TEST_CRASH)
    // Rollback test build only (-D OTA_TEST_CRASH): crash 5 s after boot, before the image confirms itself, so the
    // bootloader must fall back to the previous image on the next boot (step 11d's gate).
    if (nowMs > 5000UL) {
        out.println("OTA test_crash aborting before confirm");
        out.flush();
        abort();
    }
#endif
#if defined(OTA_TEST_HANG)
    // Reboot-deadline test build only (-D OTA_TEST_HANG): the loop hangs 5 s after boot without crashing, so the
    // image never confirms; the deadline timer must reboot it and the bootloader roll back (review 12.0 L-OTA2).
    if (nowMs > 5000UL) {
        out.println("OTA test_hang blocking the loop before confirm");
        out.flush();
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#endif
    // Not ready (the Node's radio did not start): no confirm, so the reboot deadline rolls the image back instead of
    // leaving a Node that NET * OTA can no longer reach (deep review N-M-A, the owner's L-OTA2 rule).
    if (gConfirmChecked || nowMs < kConfirmAfterMs || !ready) {
        return;
    }
    gConfirmChecked = true;
    const esp_partition_t* running = nullptr;
    if (runningImagePendingVerify(&running)) {
        esp_ota_mark_app_valid_cancel_rollback();
        if (gDeadlineTimer != nullptr) {
            esp_timer_stop(gDeadlineTimer);
            esp_timer_delete(gDeadlineTimer);
            gDeadlineTimer = nullptr;
        }
        out.print("OTA confirmed partition=");
        out.println(running->label);
    }
}

}  // namespace ota

#endif  // ARDUINO_ARCH_ESP32
