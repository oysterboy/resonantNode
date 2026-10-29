# 2026-10-29: step 13.3, per-hop attenuation at 0.5 FS (hop10, one placement)

Same placement and firmware as 2026-10-29-step13-levelladder (79 cm, 0e8b3bd). PULSES att= per run (dB per pulse,
hop order 2950, 3200, 3450, 3700, 3950), 20 trials each.

| run | att dB | expected | strength (median) | duration (median) | shape (first-last, distinct) |
|---|---|---|---|---|---|
| mid10   | 0,0,10,0,0  | 20 / 20 | 14514 | 217 | 2950-3700, 5 |
| mid15   | 0,0,15,0,0  | 20 / 20 | 14518 | 113 | 2950-3700, 4 |
| mid20   | 0,0,20,0,0  | 20 / 20 | 14521 | 113 | 2950-3700, 4 |
| mid30   | 0,0,30,0,0  | 20 / 20 | 14524 | 113 | 2950-3700, 4 |
| first20 | 20,0,0,0,0  | 20 / 20 | 14507 | 182 | 3200-3700, 4 |
| two15   | 0,15,0,15,0 |  0 / 20 | 11036 |  69 | none: duration_too_short |
| two20   | 0,20,0,20,0 |  0 / 20 | 10988 |  69 | none: duration_too_short |

Lab: docs/lab/experiments/E005_margin.md.
