# 2026-10-16 first sweep on hardware, 70 cm (tools/bench/sweepcheck.sh)

E3 Emitter flashed to f2dccc2 (CHIRP `sweep=`, I2sToneOutput::setSweep: linear phase-continuous glide inside one
chirp; the flash needed BOOT held, the board does not enter download mode on its own), E2 Analyzer 5ba805d (shipped
defaults), band 2950-3950, boards at the bandcheck-70cm placement, SEQ DIAG on, 10 trials per cell.

| Cell | expected | detector view | amp P75 | contrast P75 |
|---|---|---|---|---|
| sweep 2950 -> 3950 in 100 ms | **0/10** | duration 123-124 ms, coverage 99-104, all `pattern_rejected` | 2,727-3,129 | 28,996-29,932 |
| sweep 3950 -> 2950 in 100 ms | **0/10** | 126-127 ms, coverage 101-105, all `pattern_rejected` | 2,851-2,993 | 29,147-29,421 |
| sweep 2950 -> 3950 in 150 ms | **0/10** | 181-186 ms, coverage 144-147, all `pattern_rejected` | 2,691-2,790 | 30,094-30,480 |
| chirp 3,200 Hz, 100 ms (reference) | 10/10 | - | 2,347-3,006 | 32,756-32,758 |

The detector accepts the sweep as one occurrence of the right length (the glide through the comb leaves no gap),
but the contrast inspector rejects it: a tone gliding across 1,000 Hz sits between two bins most of the time, so
its P75 over the first 100 ms is 29,000-30,500, under the 30,500 limit that keeps radio music out (that music
reached 30,085 the same evening). The slower sweep (10 Hz/ms) comes closest. With this contrast definition the
detection layer's robust shape is the bin-aligned hop train; a sweep needs either a stepped (bin-aligned) version,
a lower limit (no margin left), or a contrast that follows the peak bin inside the window (DET-015 territory).
Sweep captures for offline work: `2026-10-16-corpus-70cm/sweep2950to3950_{100,150}ms_pos1_*`. Host replay of them:
the 100 ms capture rejected on contrast (as on the board), the 150 ms capture accepted, i.e. that shape sits right at
the 30,500 limit (board cell 30,094-30,480, this trial above it).
