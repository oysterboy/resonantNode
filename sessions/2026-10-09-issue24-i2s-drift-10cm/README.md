# 2026-10-09 issue #24, I2S drift classification, piezo analyzer

Purpose: classify the slow drift in the raw INMP441 stream (issue #24,
`docs/refactors/i2s-first-difference-revisit.md` sections 6 and 8).
Analyzer `464b8fc` (80 MHz QIO flash, sample-clock fix in place; decode and
preprocess unchanged), `RAW ... mode=i2s` (decoded words straight from the
driver, bypassing First Difference). Quiet room, emitter in remote mode.
Analysis: `tools/logging/raw_capture_slope.py`.

Capture-size limit found: RAW always allocates a 6,000-row capture buffer
(72 KB) plus the pre-trigger ring, and the largest free heap block is
110 KB, so issue #24's `pre=500 post=200` (and `pre=300`) fail with
`raw_buffer_alloc_failed`. `pre=150` runs, but the pre ring delivered only
256 samples, fewer than the script's 512 minimum, so the `pre150` captures
are analysed over the whole window, chirp included. The clean measurement is
the `post350` set: 350 ms after the trigger, analysed only from
`emit_done + 20 ms` to the end (`*_quiettail` copies, 3,680-3,781 samples,
about 0.23 s).

| Capture | Window analysed | dc (PCM) | slope 1k->7k |
|---|---|---|---|
| pre150_1 | all (chirp inside) | 14,776 | -0.4 dB/oct |
| pre150_2 | all (chirp inside) | -69,167 | -1.0 dB/oct |
| pre150_3 | all (chirp inside) | -40,825 | -0.7 dB/oct |
| post350_1 quiet tail | after chirp | -43,680 | -1.9 dB/oct |
| post350_2 quiet tail | after chirp | -6,913 | -1.2 dB/oct |
| post350_3 quiet tail | after chirp | -15,525 | -1.5 dB/oct |

Band levels of the quiet tails (raw, dB): 125-250 Hz ~104, 250-500 ~98,
500-1k ~91, 1-2k ~89, 2-4k ~90.5, 4-8k ~86. Flat above 1 kHz, rising about
5-6 dB/octave below it.

Reading: an integrating stage would fall about 6 dB/octave across the whole
band. The floor is flat from 1 to 7 kHz, so nothing between the mic and
decode integrates; the drift is low-frequency content (mic DC offset and
1/f noise, room LF). Per issue #24 this points to a DC blocker / high-pass
as the eventual replacement for First Difference (a retune, after the field
trial), with the D-AMP HAL carrying First Difference unchanged meanwhile.

Not done here (needs the owner): clap test, capture on a second piezo node.
The drift (`dc`) wanders between captures (-69k to +15k within minutes).
