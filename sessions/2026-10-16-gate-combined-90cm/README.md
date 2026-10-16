# 2026-10-16 5b gate battery, 90 cm, combined build

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell (tools/bench/gate5b.sh). Companion sessions: 2026-10-16-gate-combined-{110,100,90,80,70,60}cm.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 (5a reference) | 10/10 | 2,994-3,330 | 32,751-32,755 |
| hop3 30x3 gap 10 | 10/10 | 3,005-3,277 | 32,667-32,680 |
| hop5 20x5 gap 5 | 10/10 | 2,760-3,264 | 31,835-32,071 |
| hop5 20x5 gap 15 (max_dur 300) | 10/10 | 2,695-2,846 | 31,667-31,947 |
