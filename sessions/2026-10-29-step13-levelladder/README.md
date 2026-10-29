# 2026-10-29: step 13.2, level ladder (hop10, one placement)

E3 Emitter -> E2 Analyzer at 79 cm, the 2026-10-26 layout (unchanged; remote session). Both on 0e8b3bd (PULSES
level=). hop10 at the shipped band, 20 trials per rung, mode=detail. (seqrun first recorded 110 cm from an old
session's defaults; corrected to 79 in session.json and the index the same evening.)

| level FS | dB vs 0.5 | expected | source_strength (median) | rejects |
|---|---|---|---|---|
| 0.5  |  0.0 | 20 / 20 | 14455 | |
| 0.35 | -3.1 | 20 / 20 | 10126 | |
| 0.25 | -6.0 | 11 / 20 |  7209 | 9 pattern_rejected: amp observed weak, medium required (shape strong) |
| 0.21 | -7.5 |  0 / 20 |  6024 | coverage_too_low |
| 0.18 | -8.9 |  0 / 20 |  5136 | duration_too_short |
| 0.15 | -10.5 | 0 / 20 |  4218 | duration_too_short |
| 0.12 | -12.4 | 0 / 20 |  3343 | duration_too_short |

Strength scales with the level (linear, no saturation at 79 cm). Lab: docs/lab/experiments/E005_margin.md.
