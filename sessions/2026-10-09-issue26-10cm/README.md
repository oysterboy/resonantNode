# 2026-10-09 issue #26, 10 cm

Purpose: find and fix the empty-history inspections (DET-009). Analyzer on
COM6, piezo emitter `62a949e` over UART2, 10 cm, vertical emit / vertical
mic. Signal was lower than on 2026-10-08 (TonalPulseFreq strength median
12,800 vs 17,300, boards likely moved). Write-up:
`docs/refactors/archive/i2s-sample-clock.md` on `main`.

Runs by prefix (firmware hash in each file name):

| Prefix | Firmware | What |
|---|---|---|
| `R_` | 05da661 | instrumented, reproducer attempts; no failures at light load |
| `R_loopdelay10_` | 05da661 + `-DTEST_LOOP_DELAY_MS=10` | forced backlog: 2/30 incomplete, 15,600 out-of-order history records |
| `F_` | 705f120 | sample clock v1 (nominal rate): fixes holes, onsets early, trials `unexpected` |
| `F2_` | 846dd0b | v2 with late-block resync heuristic: resync fired 660x, worse |
| `F3_`, `F4_` | ce2e3e8, 6550714 | dropped-buffer counting from the driver: onsets correct vs planned trigger; drops revealed |
| `F5_` | ce9c62d | + 16 KB serial TX buffer: drops persist (not serial-bound) |
| `G_evq_on/off` | 1f45158 (+ flag) | read rate ~9,800/s with or without event queue: CPU-bound |
| `H_cost*`, `H_freqprof*`, `H_qio80*`, `H_dio80*` | f72bd69 (+ flags / flash settings) | 79 us/sample at 40 MHz DIO, 58 at 80 MHz DIO, 51 at 80 MHz QIO |
| `V_` | 464b8fc | verification: Scalar 50/50 expected, Freq reproducer 0 incomplete, 0 / 1,421 dropped buffers |

Conclusion: the Analyzer could not keep up at 16 kHz from 40 MHz DIO flash,
the I2S driver dropped buffers, nothing counted them, and read-time block
stamps turned the loss into holes in FeatureHistory. Fixed by an
index-based sample clock with drop accounting and 80 MHz QIO flash.

Side finding: trial `dt` is measured against the EMIT_START marker as the
Analyzer polls it (~38 ms late); with true sample time, onsets show
`dt=-38` while onset minus planned trigger is about +63 ms (the CHIRP is sent
~60 ms late in detail mode). Compare `dt` only within one firmware.
