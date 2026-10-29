# Platform: stay on Arduino-ESP32 2.0.17, or move (Arduino 3.x, Arduino as an ESP-IDF component, pure ESP-IDF)

```text
Status:  parked (2026-10-30)
Became:  roadmap-0-steps.md "Carried over", at the end, no step yet (owner 2026-10-30)
Lab:     none; a reading of platformio.ini, src/ (main 2405b9e5 + 12.0.1) and the 12.0 review
         (docs/research/code-review-2026-10-29.md)
```

## Question (owner, 2026-10-29)

"Moving off Arduino dependencies: wasn't that planned somewhere, and should we do it now?" It was not planned as a
step. What exists: detection builds on the host without Arduino (test/native_shim, enforced by the host build);
roadmap-node.md notes small injections (Behavior's `random()`); freertos-tasks.md discusses leaving the single
`loop()`. Answer given: not now (it needs the owner at the bench, it is a migration, step 12 is checking the current
build); this note prepares the decision, after step 14, with the owner on site.

## Where Arduino is today

`platform = espressif32@6.13.0`, `framework = arduino`: Arduino-ESP32 2.0.17 on ESP-IDF 4.4.7.

| Area | Arduino use | Already direct IDF |
|---|---|---|
| src/hal (I2S mic, I2S tone, piezo) | `delay`, `ledc*` (piezo only) | the legacy I2S driver (`i2s_driver_install`, `i2s_read`, `i2s_write`) |
| src/net (ESP-NOW) | `WiFi.mode`, `WiFi.disconnect` | `esp_now_*`, `esp_wifi_set_channel`, FreeRTOS queue |
| src/app/FirmwareOta | `WiFi.begin`, `HTTPUpdate`, `ESP.restart` | `esp_ota_*`, rollback |
| src/app/NvsParamStore | `Preferences` | - |
| src/modes (20 files) | `Serial`, `Serial2`, `delay`, `millis` / `micros` (3 each), `random` | `esp_timer_get_time` (TimeBase) |
| src/detection, behavior, output | `<Arduino.h>` for types and `serialOut()` printing; no hardware | the logic is host-built |

So the hardware paths already sit on IDF calls; Arduino supplies Serial / Serial2, WiFi, HTTPUpdate, Preferences,
`delay` and the `setup()` / `loop()` runtime.

## What Arduino has cost (on record)

- Serial: every write queued as its own TX ring item (2026-10-22, decisions/2026-10-22-serial-lines-and-iram-hot-
  path.md): dropped DMA buffers until lines were built whole.
- WiFi: auto-reconnect on by default (review M1: a failed OTA could keep the radio hopping), credentials persisted to
  NVS by default (L-OTA4).
- OTA: the core marks every image valid at boot unless `verifyRollbackLater()` is overridden (found in 11d).
- No task watchdog on `loop()` by default (L-OTA2: a hung image never reboots).
- Version: 2.0.17 / IDF 4.4.7 is the last 2.x; new fixes land in 3.x on IDF 5.

## Options

1. **Stay on 2.0.17**, keep confining Arduino to the edges (hal, app, the mode files' serial). Cost: none now; the
   known quirks are worked around one by one.
2. **Arduino-ESP32 3.x (IDF 5.1+)** via the pioarduino platform (PlatformIO's official espressif32 platform stays on
   2.x). Cost: the legacy I2S driver is deprecated in IDF 5 (new `i2s_std` channel API: rewrite hal's I2S, the most
   timing-sensitive code); `ledc` API changed (piezo build); ESP-NOW receive callback signature changed; HTTPUpdate
   and WiFi mostly the same. Gain: current fixes, IDF 5's drivers, still `Serial` and `setup()` / `loop()`.
3. **Arduino as an ESP-IDF component** (ESP-IDF project, Arduino linked for the parts that use it). Gain: menuconfig
   (task watchdog, log levels, rollback options, partition table) while keeping Serial / WiFi / Preferences. Cost: the
   build moves to ESP-IDF's CMake (PlatformIO supports `framework = espidf, arduino`); bigger toolchain change.
4. **Pure ESP-IDF.** Replace Serial (UART driver + a small line writer), WiFi (esp_wifi + esp_netif), HTTPUpdate
   (esp_https_ota, which also brings HTTPS and image validation), Preferences (nvs), `delay` / `millis` (vTaskDelay /
   esp_timer). Gain: full control, the native OTA and watchdog. Cost: the largest; every mode's serial path and the
   test shim's assumptions change.

All options except 1 change the bootloader and possibly the partition table: a USB flash of every board, with a
real chance one needs hands-on recovery. Not before the owner is on site.

## Suggested path

- Now (no board risk): keep new code off Arduino where it costs nothing: a `SerialOut`-style writer behind one
  interface (the modes already print through `serialOut()`), time through TimeBase, NVS behind NvsParamStore,
  `random` injected. Each makes options 2-4 smaller.
- Decide after step 14 (owner on site): option 2 is the likely middle (current IDF, same programming model, the I2S
  rewrite is the one big piece); option 3 if menuconfig control (watchdog, rollback) is wanted without rewriting
  Serial / WiFi. If adopted, it becomes its own step before BEH-008 / BEH-009 (16 / 17), so the new behaviour is built
  on the final platform, gated by the standing checks (nodecheck.py, analyzerab.py, a soak, the OTA round trip).

## Open questions

- Does the I2S full-duplex setup (mic and amp on one port, D-AMP) carry over to IDF 5's channel API without the
  timing problems of 2026-10-09 (clock backsteps, NODE-019)?
- ESP-NOW and WiFi STA coexistence (OTA's channel switch) under IDF 5.
- Host build: does test/native_shim stay valid (it stands in for `<Arduino.h>` only)?
