# 2026-10-16 5b gate battery, 70 cm, combined build

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell (tools/bench/gate5b.sh). Companion sessions: 2026-10-16-gate-combined-{110,100,90,80,70,60}cm.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 | 0/10 | - | - |
| hop3 30x3 gap 10 | 10/10 | 3,503-3,676 | 32,665-32,680 |
| hop5 20x5 gap 5 | 10/10 | 3,577-3,655 | 32,199-32,283 |
| hop5 20x5 gap 15 (max_dur 300) | 10/10 | 2,752-2,850 | 32,361-32,399 |

The fixed chirp: detector sees 24-25 ms of the 100 ms tone (duration_too_short), 3,200 Hz is in a comb null at 70 cm today (10/10 on 2026-10-15). Every hop passes.
