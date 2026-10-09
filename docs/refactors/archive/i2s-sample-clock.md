Archived: 2026-10-09 — DET-009 / issue #26 fixed and hardware-verified on the piezo bench: I2S blocks are stamped from the sample index (clock locked to micros(), dropped DMA buffers counted from I2S_EVENT_RX_Q_OVF), flash runs at 80 MHz QIO so the Analyzer keeps up (~54 us of a 62.5 us budget per sample), dropped buffers feed overflowCount. 0 empty-history inspections in 100 trials. Still open: ~14% CPU headroom only (detail mode + diagnostics still drops audio between trials), trial dt is anchored on the late-polled EMIT_START marker, D-AMP HAL (#19) must keep stampBlock() and the drop accounting. Not re-verified: D-AMP boards, long runs (issue #27 soak).

# I2S sample clock and audio loss (DET-009, issue #26)

Status: closed 2026-10-09 (single-thread pass). Roadmap step 1a, DET-009 in
`docs/roadmaps/roadmap-detection.md`. Evidence: branch `bench`, session
`2026-10-09-issue26-10cm` (and the issue #7 sessions of 2026-10-08).

## Symptom

An accepted occurrence was inspected against a FeatureHistory window that
did not cover the requested range (`MagnitudeInspectionNote::
HistoryWindowIncomplete`), so the verdict rejected a strong chirp. Seen in
2-12 of 50 TonalPulseScalar trials on the pre-cleanup firmware, 0 after
`795f649`, but 14 of 30 TonalPulseFreq trials with diagnostics on.

The `inspect.available_start_ms=0 available_end_ms=0` in those SEQ_INSPECT
lines is a reporting artifact, not an empty window: the inspector fills the
available range only for a valid window. `HistoryWindowIncomplete` means the
window had values but a missing edge.

## Mechanism (measured, not inferred)

Three things stacked. Each was confirmed with instrumentation on the bench
(`SEQ_HISTDBG` line, commits `05da661` onward).

1. **The Analyzer could not keep up with 16 kHz.** Its per-sample path
   (AudioSignal, FreqBandStream, DetectionRuntime, FeatureHistory, Analyzer
   bookkeeping) runs mostly from flash through the cache. At the board
   default (40 MHz DIO) it cost about 79 us per sample against a budget of
   62.5 us. The loop fell behind and the I2S driver dropped whole DMA
   buffers: about 6,000 of 16,000 samples per second. The cost depends on
   code placement in flash, which is why unrelated small commits (and
   `795f649`, less work per sample) moved the failure rate up and down.
2. **Nothing reported the loss.** `AudioSourceStats::overflowCount` had no
   producer, so the Analyzer's `buffer_overrun` trial class never fired.
3. **Read-time timestamps turned the loss into history holes.** Each block
   was stamped as if its last sample arrived when the block was read. Late
   reads jumped sample time forward, catch-up reads jumped it back (about
   7,000 backward steps per 100 s, up to 4.5 ms; with a 10 ms loop delay,
   15,600 out-of-order history records in 30 trials). FeatureHistory bins
   by that time, so the window around a chirp had holes; a hole at either
   edge made it incomplete. The old stamps also hid the loss: they always
   tracked wall-clock time, so dropped audio never showed as a gap.

Supporting measurements (amp-window coverage of observed inspections):
median 0.81 / 0.77 before `795f649`, 0.97 at it, 1.00 on current firmware
with light load, 0.68 for TonalPulseFreq with diagnostics on.

## Fix

- **Sample clock** (`705f120`, `846dd0b`, `ce2e3e8`):
  `AudioSourceI2S::stampBlock()` stamps a block from its sample index,
  `anchorUs + (index - anchorIndex) * usPerSample`. Every 0.5 s the anchor
  absorbs the minimum read latency seen in that window and half of it goes
  into `usPerSample` (measured rate 16,000.6 Hz on the piezo analyzer).
  Dropped DMA buffers are counted from `I2S_EVENT_RX_Q_OVF` (driver event
  queue sized for one second of events) and advance the index, so a loss
  is an honest gap at the right time, not a lag. An intermediate heuristic
  (`846dd0b`, resync on a block older than the DMA queue) was replaced in
  `ce2e3e8`: it re-anchored on blocks that were still late.
- **CPU headroom** (`464b8fc`): flash at 80 MHz QIO for every env. Measured
  on the analyzer, 6 trials each: 40 MHz DIO 79 us/sample with drops,
  80 MHz DIO 58 us with 3-4% drops, 80 MHz QIO 51 us with none.
- **Honest accounting** (`464b8fc`): dropped buffers also feed
  `overflowCount`, so a trial with lost audio is `buffer_overrun`, not a
  verdict rejection. Node `RB STATUS` prints `i2s.samples_read`,
  `i2s.dropped_dma_buffers`, `i2s.clock_rate_mhz`.
- **Serial TX buffer** (`ce9c62d`): 16 KB for the Analyzer so a detail-mode
  report queues instead of blocking. Not the cause of the drops (they
  persisted with 760 bytes of output per trial), but removes a stall source.

Not a fix, and not done: widening inspection windows, retrying, or passing
`HistoryWindowIncomplete`.

## Side findings

- **Trial `dt` is measured against the emitter's EMIT_START marker as the
  Analyzer polls it**, and the poll runs after the sample loop. With an
  accurate sample clock, onsets read about 38 ms *before* that anchor
  (`dt=-38`), while onset minus the planned trigger is +63 ms, which matches
  the chirp command being sent ~60 ms late. The old read-time stamps were
  late by the same polling delay, so the two errors cancelled. Follow-up:
  anchor `dt` on the time the CHIRP command was sent, or timestamp the
  marker on receipt; until then compare `dt` only within one firmware.
- **The Analyzer sends the CHIRP command ~60 ms after the planned trigger**
  in detail mode, because the previous trial's report prints first.
- The first emitter remote claim after each Analyzer boot times out.

## Verification (2026-10-09, piezo analyzer + emitter at 10 cm, firmware `464b8fc`)

| Run | Result | Empty-history inspections | Dropped DMA buffers | us/sample |
|---|---|---|---|---|
| TonalPulseScalar, 50 trials, diag off | 50 expected | 0 | 0 | 53.8 |
| TonalPulseFreq, `freqScore=12000`, diag on, 50 trials (the reproducer) | 50 rejected (amp class `none`, today's lower signal) | 0 | 1,421 | 53.0 |

- Amp-window coverage 1.000 in all 50 reproducer trials and no trial marked
  `buffer_overrun`: the reproducer's drops fall between trials, while the
  detail-mode report is formatted, not inside an inspection window.
- Sample clock: measured rate 15,999.97-16,000.0 Hz, largest anchor
  correction 0.6 ms (Freq run) and 5.6 ms (Scalar run), 1-4 out-of-order
  history records per run (was hundreds to 15,600).
- T1: all three envs build. T4: 9/9 pass on-device.
- Node (`env:esp32dev`, emitter on AUTO at 10 cm): `i2s.dropped_dma_buffers`
  15 during boot, +5 over the next 60 s; `i2s.clock_rate_mhz` 15,999,968.
- Before the fix, same bench: 0-14 empty-history inspections per 30 trials
  depending on load and build; 6,000 samples/s dropped at 40 MHz DIO.

## Open

- Per-sample headroom is about 14% at 80 MHz QIO (53-54 of 62.5 us). A larger margin needs
  hot-path work (IRAM placement or block-wise processing). Track if drops
  reappear (`dropped_dma_buffers` in SEQ_HISTDBG, `i2s.dropped_dma_buffers`
  in RB STATUS).
- The `dt` anchor follow-up above.
- The D-AMP HAL (#19) reads I2S full-duplex at the same 16 kHz; it must
  keep `stampBlock()` and the dropped-buffer accounting.
