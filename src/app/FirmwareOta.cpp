#include "FirmwareOta.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <string.h>

#include "TimeBase.h"
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
bool gConfirmChecked = false;

// The value of url= up to the end of the line or the next space, into `out`.
bool parseUrl(const char* line, char* out, size_t capacity) {
    const char* p = strstr(line, "url=");
    if (p == nullptr) {
        return false;
    }
    p += 4;
    size_t n = 0;
    while (p[n] != '\0' && p[n] != ' ' && n + 1 < capacity) {
        out[n] = p[n];
        ++n;
    }
    out[n] = '\0';
    return n > 7 && strncmp(out, "http://", 7) == 0;
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
    char url[160];
    if (!parseUrl(line, url, sizeof(url))) {
        out.println("OTA result=bad_command (use OTA url=http://<host>:<port>/<file>.bin)");
        return Result::BadCommand;
    }
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
    out.println("OTA result=no_credentials (include/wifi_secrets.h missing at build time)");
    return Result::NoCredentials;
#else
    out.print("OTA start url=");
    out.println(url);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long startMs = timebase::nowMs();
    while (WiFi.status() != WL_CONNECTED) {
        if (timing::elapsedSince(timebase::nowMs(), startMs, kWifiTimeoutMs)) {
            out.println("OTA result=wifi_timeout");
            WiFi.disconnect();
            return Result::WifiTimeout;
        }
        delay(100);
    }
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
    WiFi.disconnect();
    return none ? Result::NoUpdate : Result::DownloadFailed;
#endif
}

void confirmRunningImage(unsigned long nowMs, Print& out) {
    if (gConfirmChecked || nowMs < kConfirmAfterMs) {
        return;
    }
    gConfirmChecked = true;
    esp_ota_img_states_t state;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        out.print("OTA confirmed partition=");
        out.println(running->label);
    }
}

}  // namespace ota

#endif  // ARDUINO_ARCH_ESP32
