# Philips I2S Boundary Spike Report

Date: 2026-06-19

## Summary

This pass investigated the repeatable `sampleIndex % 64 == 32` discontinuity in the PHILIPS I2S input path.

The current evidence does not show phase 32 as a uniquely bad boundary. Across the 10-run Philips campaign, phase 32 is near the overall average and ranks far below the noisiest phases.

The strongest supported conclusion is:

- the spike is not proven to be a decode-only issue
- the spike is not proven to be a raw-word-only issue
- phase 32 is not statistically special in the aggregate Philips campaign
- the next best test is still a controlled read-size / read-path comparison

## Installed Versions

| Component | Version | Evidence |
| --- | --- | --- |
| PlatformIO platform | `espressif32 6.13.0` | `platformio.ini` and `~/.platformio/platforms/espressif32/platform.json` |
| Arduino-ESP32 framework | `3.20017.241212+sha.dcc1105b` | `~/.platformio/packages/framework-arduinoespressif32/package.json` |
| Arduino ESP32 package | `2.0.17` | `~/.platformio/packages/framework-arduinoespressif32/installed.json` |
| Arduino I2S library | `1.0` | `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/library.properties` |
| ESP-IDF headers used by this framework | `4.4.7` | `~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/include/esp_common/include/esp_idf_version.h` |
| Toolchain | `xtensa-esp32-elf 8.4.0+2021r2-patch5` | `~/.platformio/platforms/espressif32/platform.json` |

Relevant build flags from `platformio.ini`:

- `ANALYZER_MODE` for `esp32dev-analyzer`
- `CHIRP_FREQUENCY_HZ=3200`
- `BUILD_DATE="2026-05-26"`
- `BUILD_REV=1`
- `ARDUINO_LOOP_STACK_SIZE=12288`

## Exact RX Path

The compiler is using the bundled Arduino I2S wrapper:

- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.h`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp`

Current path:

```text
I2S.begin(I2S_PHILIPS_MODE, 16000, 32)
  -> Arduino I2S wrapper
  -> IDF legacy I2S driver
  -> Arduino ring buffer
  -> I2S.available()
  -> I2S.read(rawBytes, 512)
  -> AudioSourceI2S::refillBlock()
  -> slot selection
  -> decodePcmSample()
  -> RAW capture / detector input
```

Important arithmetic in the current Philips path:

- wrapper DMA buffer size default: `128` frames
- wrapper DMA buffer count: `2`
- one DMA buffer: `128 * 2 * 4 = 1024` bytes
- two DMA buffers: `2048` bytes total ring-buffer capacity
- `AudioSourceI2S::refillBlock()` caps reads at `128 * 4 = 512` bytes
- `512` bytes = `128` 32-bit words = `64` stereo frames
- `64` selected mono samples are emitted per full 512-byte Philips read

## Key Code Locations

### AudioSourceI2S

- `src/hal/AudioSourceI2S.cpp:98` - switches between `I2S_LEFT_JUSTIFIED_MODE` and `I2S_PHILIPS_MODE`
- `src/hal/AudioSourceI2S.cpp:210` - `readRawSample()` debug-only path
- `src/hal/AudioSourceI2S.cpp:297` - `philipsDiagnostics()` accessor
- `src/hal/AudioSourceI2S.cpp:349` - `refillBlock()` starts the Philips read/decode path
- `src/hal/AudioSourceI2S.cpp:384` - `I2S.read(rawBytes, requestedBytes)`
- `src/hal/AudioSourceI2S.cpp:415` - `decodePcmSample(samplePtr, bytesPerSample)`
- `src/hal/AudioSourceI2S.cpp:532` - Philips diagnostics become active in the selected-slot path
- `src/hal/AudioSourceI2S.cpp:545` - per-phase delta aggregation
- `src/hal/AudioSourceI2S.cpp:555` - max-delta capture

### Analyzer RAW capture

- `src/detection/analyzer/tools/AnalyzerRawCapture.cpp:98` - compact `RAW_PHILIPS_SUMMARY`
- `src/detection/analyzer/tools/AnalyzerRawCapture.cpp:121` - per-phase `RAW_PHASE` lines
- `src/detection/analyzer/tools/AnalyzerRawCapture.cpp:448` - RAW PCM capture uses `readSample()`
- `src/detection/analyzer/tools/AnalyzerRawCapture.cpp:715` - prints the Philips diagnostics summary after capture

### Arduino I2S wrapper

- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:25` - `_I2S_DMA_BUFFER_COUNT = 2`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:142` - `channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:143` - `communication_format = I2S_COMM_FORMAT_STAND_I2S`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:145` - `dma_buf_count = _I2S_DMA_BUFFER_COUNT`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:297` - internal ring-buffer byte size calculation
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:552` - `available()` reports ring-buffer occupancy in bytes
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:593` - `read(void* buffer, size_t size)`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:610` - `xRingbufferReceiveUpTo(...)`
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:889` - RX DMA completion callback path
- `~/.platformio/packages/framework-arduinoespressif32/libraries/I2S/src/I2S.cpp:962` - `_post_read_data_fix()`

## Diagnostic Results

Campaign file:

- `logs/raw_pcm_philips_series_2026-06-19.txt`

All 10 captures completed with:

- `captured=4000`
- `dropped=0`
- `OK RAW`

### Per-run max delta summary

| Run | max_delta_abs | max_delta_mod64 | read_idx | selected_in_read |
| --- | ---: | ---: | ---: | ---: |
| 1 | 79220 | 29 | 21 | 29 |
| 2 | 63706 | 48 | 22 | 48 |
| 3 | 82570 | 26 | 21 | 26 |
| 4 | 75428 | 5 | 21 | 5 |
| 5 | 58452 | 54 | 22 | 54 |
| 6 | 60122 | 0 | 26 | 0 |
| 7 | 60482 | 56 | 22 | 56 |
| 8 | 63732 | 0 | 22 | 0 |
| 9 | 219710 | 0 | 28 | 0 |
| 10 | 62742 | 47 | 23 | 47 |

Observations:

- the largest jump in the 10-run series was not phase 32
- phase 0 produced the biggest outlier in run 9
- several different phases own the max delta across runs

### Aggregate phase statistics

| Phase | Captures | Avg mean_abs_delta | Avg max_abs_delta | Notes |
| --- | ---: | ---: | ---: | --- |
| 0 | 10 | 12994.1 | 72280.8 | highest average phase mean |
| 5 | 10 | 8712.9 | 65125.4 | above average |
| 26 | 10 | 8593.9 | 63651.4 | above average |
| 20 | 10 | 8540.9 | 53191.6 | above average |
| 15 | 10 | 8513.0 | 54146.8 | above average |
| 32 | 10 | 8052.2 | 58191.4 | close to average, rank 46/64 |

Overall mean of phase means:

- `8248.2`

Phase 32 ranking:

- `46 / 64` by average `mean_abs_delta`

## Conclusion

The current campaign does not support "phase 32 is uniquely broken" as a strong conclusion.

What the data does support:

- the Philips stream is real and stable enough for statistical comparison
- the biggest discontinuities occur at multiple phases, not just phase 32
- phase 32 is not the standout phase in the aggregated campaign
- the current evidence is still consistent with a read-boundary or wrapper-boundary issue, but it is not proven

## Smallest Next Test

Run the same Philips capture with only one variable changed at a time, in this order:

1. direct ESP-IDF `i2s_read()` versus Arduino `I2S.read()`
2. change `I2S.setBufferSize()` / DMA geometry
3. then change application read size: `256`, `512`, `1024`

Keep a continuous global sample index across all tests.

The decisive question is whether the discontinuity moves when the transport path or DMA geometry changes. A phase shift from read-size-only changes is not by itself conclusive, because `I2S.read()` only drains bytes from the Arduino ring buffer and can re-index an already-fixed boundary.
