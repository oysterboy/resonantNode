# Platform: off Arduino, onto ESP-IDF

Status: decided (owner 2026-10-30: "Platform: off arduino"); not scheduled (owner 2026-10-30: at the end of the
roadmap, no step yet).
Date: 2026-10-30.

## Decision

The firmware leaves the Arduino-ESP32 core for plain ESP-IDF (option 4 of
docs/research/platform-arduino-vs-esp-idf.md): Serial / Serial2 become the UART driver behind the existing line
writer, WiFi becomes esp_wifi + esp_netif, HTTPUpdate becomes esp_https_ota, Preferences becomes nvs, `delay` /
`millis` become vTaskDelay / esp_timer, and `setup()` / `loop()` become app_main and its task. The legacy I2S driver
moves to IDF 5's channel API on the way.

When: not now. It stays at the end of roadmap-0-steps.md's "Carried over" list with no step until the owner
sequences it; it needs every board flashed over USB (bootloader and partition table), so the owner on site.

## Why

The hardware paths already call IDF directly (I2S, ESP-NOW, OTA partitions, esp_timer). What Arduino adds has cost
time on record: Serial's per-write TX items (dropped DMA buffers until lines were built whole), WiFi auto-reconnect
and persisted credentials (review 12.0 M1, L-OTA4), images marked valid at boot (step 11d), no loop watchdog
(L-OTA2). Arduino-ESP32 2.0.17 / IDF 4.4.7 is the last of its line.

## Until then (no board risk, applies now)

New code stays off Arduino where that costs nothing: print through `serialOut()` / `printKeyValue`, time through
TimeBase, storage through NvsParamStore, randomness injected, no new `<Arduino.h>` include outside hal, app and the
mode files. Each of these makes the move smaller. Detection, Behavior and output keep building on the host without
Arduino (test/native_shim, enforced by the host build).

## Rules out

Staying on Arduino 2.0.17 for good, and the middle options (Arduino 3.x via pioarduino, Arduino as an IDF
component) as the target. If the move is ever split into stages, Arduino 3.x may be a stepping stone only.

## Revisit

When it is sequenced: confirm the D-AMP full-duplex I2S timing on IDF 5's channel API (the 2026-10-09 clock
backsteps, NODE-019), ESP-NOW with the OTA's STA channel switch, and whether the host shim still covers what it
stands in for.
