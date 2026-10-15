# 2026-10-15 DET-011: PULSES timing and grid, default scalar params (main ce586c1 + threshold fix)

E2 Analyzer (COM5), E3 Emitter with PULSES, 0.5 FS (COM3), 3,300 Hz, 90 cm, up. `SEQ start ... freq=3300
dur=PULSE_MS pulses=3 gap=GAP_MS`, 10 trials per cell, default scalar params (max_gap 20, max_duration 200,
release debounce 10). Runs before/after the P3x names: P3x15_g20 is the timing check.

| cell | result | detector sees (median dur / coverage / longest island / max gap, ms) |
|---|---|---|
| 15 ms, gap 5 | 10/10 expected | 108 / 102 / 32 / 2 |
| 15 ms, gap 20 | 10/10 expected | 136 / 111 / 31 / 7 |
| 15 ms, gap 60 | 0/10 duration_too_short | 49 / 35 / 29 / 8 (first pulse only) |
| 15 ms, gap 115 | 0/10 duration_too_short | 37 / 33 / 29 / 4 (first pulse only) |
| 10 ms, gap 5 | 0/10 duration_too_short | 32 / 16 / 9 / 6 |
| 10 ms, gap 20 | 0/10 coverage_too_low | 88 / 73 / 26 / 5 |
| 10 ms, gap 60 | 0/10 duration_too_short | 25 / 21 / 12 / 3 |
| 10 ms, gap 115 | 0/10 duration_too_short | 25 / 21 / 12 / 3 |

The speaker/room tail fills ~13 ms of a 20 ms gap and stretches a 15 ms pulse to a ~31 ms island; 5 ms
gaps do not show as gaps. 10 ms pulses are too short/weak at 0.5 FS.
