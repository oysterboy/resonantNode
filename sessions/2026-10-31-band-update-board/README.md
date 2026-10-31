# 2026-10-31: the band bank's update interval on the board (Analyzer, profiling build)

Analyzer E2 on 072f01cb built with `-D FREQBAND_ENABLE_PROFILING=1` (OTA, then back to the normal Analyzer image),
Emitter E3, desk at 79 cm, 0.5 FS. `SEQ FREQUPDATEEVERYSAMPLES N`, hop10 x 10 on the 5-bin band 2950-3950, then
`SEQ REPORT` (the FREQBAND profile line). report_upd<N>.log.

| N | compute calls in ~82 s | avg per compute | of which Goertzel | CPU on the bank per second | hop10 |
|---|---|---|---|---|---|
| 4 (the boards) | 327,825 | 81.6 us | 55.2 us | ~326 ms (33 %) | 10 / 10 |
| 16 | 81,981 | 82.3 us | 55.8 us | ~82 ms (8 %) | 10 / 10 |
| 32 | 40,991 | 82.3 us | 55.5 us | ~41 ms (4 %) | 10 / 10 |

Per sample at 16 kHz (62.5 us a sample): the bank costs ~20.4 us a sample at N = 4, ~5.1 at 16, ~2.6 at 32. The
profile's micros() calls add a little to each compute. The Node runs 8 band bins plus 2 guards (10 Goertzel bins)
against these 5 + 2: its per-compute cost is higher, roughly by the bin ratio on the Goertzel part (est. ~105 us,
~26 us a sample at N = 4; not measured on the Node).

With 2026-10-31-band-update-replay (every replayed verdict the same at 8 / 16 / 32): a larger interval looks free in
detection and saves a quarter or more of the CPU. Not done: a Node measurement, the margin's edge, chords, music.
Changing the boards' value is its own pass.
