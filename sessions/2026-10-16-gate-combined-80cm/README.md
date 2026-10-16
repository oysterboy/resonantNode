# 2026-10-16 5b gate battery, 80 cm, combined build

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell (tools/bench/gate5b.sh). Companion sessions: 2026-10-16-gate-combined-{110,100,90,80,70,60}cm.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 | 10/10 | 4,564-5,130 | 32,757 |
| hop3 30x3 gap 10 | 10/10 | 2,302-2,496 | 32,629-32,650 |
| hop5 20x5 gap 5 | 7/10 | 2,421-2,550 | 30,968-31,130 |
| hop5 20x5 gap 15 (max_dur 300) | 0/10 | 2,571-2,691 (5 inspected) | 29,818-30,071 (5 inspected) |

Failures are detector-side: gap 5 rejects see 79 ms / 77 ms coverage (two of five steps missing, a comb null), gap 15 sees 92-97 ms (coverage_too_low or contrast weak about 30,000 for the 5 that reached inspection). hop5 gap 5 passes with the smallest contrast margin of the day (30,968).
