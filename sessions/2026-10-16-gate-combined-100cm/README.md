# 2026-10-16 5b gate battery, 100 cm, combined build

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell (tools/bench/gate5b.sh). Companion sessions: 2026-10-16-gate-combined-{110,100,90,80,70,60}cm.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 | 10/10 | 3,741-4,262 | 32,755 |
| hop3 30x3 gap 10 | 10/10 | 3,602-3,769 | 32,672-32,685 |
| hop5 20x5 gap 5 | 10/10 | 2,795-3,002 | 31,976-32,095 |
| hop5 20x5 gap 15 (max_dur 300) | 10/10 | 2,620-2,902 | 32,268-32,392 |

2026-10-15 at 100 cm: chirp 0/10, hop5 gap 15 4/10: the comb moved.
