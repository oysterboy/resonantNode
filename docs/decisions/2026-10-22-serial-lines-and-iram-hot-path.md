# Console output goes out in whole lines; the per-sample path lives in IRAM

Status: decided and implemented 2026-10-22 (step 7a; bc9a89e, 98b2fd0).
Date: 2026-10-22.

## Decision

1. All console output goes through `serialOut()` (`src/app/SerialOut.h`), which writes one line per call to Serial.
   Nothing in `src/` calls `Serial.print/println/printf/write` or passes `Serial` as a `Print&`; input
   (`Serial.available/read`) is unchanged. The loop flushes a partial line every pass.
2. The functions every audio sample runs through carry `IRAM_ATTR`:
   - all of `AudioSignal` and `FreqBandStream`;
   - the scalar and frequency detector updates;
   - FeatureHistory recording;
   - `FieldStateTracker::update`;
   - `DetectionRuntime::observeFrame` and its per-sample helpers;
   - the Analyzer's per-sample SEQ statistics.

   A new function on that path gets `IRAM_ATTR` too. The host shim defines it empty.

## Why

Both showed up as dropped I2S buffers on the Analyzer, measured in bench/sessions/2026-10-22-ana004-timing and
-7a-gate.

1. Serial. Arduino-ESP32 2.0.17 queues every write as its own TX ring item, with a header and alignment, so small
   prints fill the 16 KB ring at ~2 KB of text. After that every call waits for the UART. A 6.6 KB SEQ report took
   ~190 ms against a 24 ms DMA queue. In whole lines it takes 3.4 ms.
2. IRAM. From flash, the per-sample chain cost ~73 us of the 62.5 us budget during SEQ, through cache misses
   across many small functions. Unrelated commits moved it over or under the line. In IRAM it costs ~37 us.

IRAM use is 67.5 KB of 128 KB at most. The Node runs the same functions and gains the same margin.

## Rules out

- Direct `Serial.print` in new code.
- Formatting on a separate FreeRTOS report task (ANA-004's earlier plan). Not needed at 3.4 ms per report.
- Deeper DMA queues as the fix for load.

## Revisit

- If IRAM gets tight (WiFi or other IRAM users).
- If the framework is upgraded: a later core may batch writes itself.
