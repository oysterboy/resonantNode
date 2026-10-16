# 2026-10-16 5b gate battery, 60 cm, combined build

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell (tools/bench/gate5b.sh). Companion sessions: 2026-10-16-gate-combined-{110,100,90,80,70,60}cm.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 | 10/10 | 2,197-2,318 | 32,733-32,748 |
| hop3 30x3 gap 10 | 6/10 | 2,037-2,150 (6 inspected) | 32,556-32,575 |
| hop5 20x5 gap 5 | 10/10 | 3,054-3,276 | 31,871-31,952 |
| hop5 20x5 gap 15 (max_dur 300) | 10/10 | 2,498-2,614 | 31,503-32,001 |

hop3 rejects (4): detector sees 58 ms / 52-54 ms coverage, one of three steps in a null, the rest is below min_duration 85 ms. Detector-side, not an inspector failure.
