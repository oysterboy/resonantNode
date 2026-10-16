# 2026-10-16 5b candidate hop shapes, 70 cm, combined build (tools/bench/hopcand.sh)

E2 Analyzer COM5, combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), E3 Emitter hop 0.5 FS, mic and speaker facing up, SEQ DIAG on, 10 trials per cell. Afternoon session (bench handed over at 14:00); the morning gate at this spacing is 2026-10-16-gate-combined-70cm. Companion sessions: 2026-10-16-hopcand-{80,90,100,110}cm, the 60 cm cell is in 2026-10-16-hopshape-60cm. Single-tone captures at the same placement: 2026-10-16-corpus-70cm.

Both hop shapes keep more than min_duration (85 ms) with one step missing: 25 ms x 5 gap 10 (2800/3050/3300/3550/3800) and a bin-aligned 30 ms x 4 gap 10 (2800/3300/3800/3050).

| Cell | expected | amp P75 | contrast P75 | detector duration |
|---|---|---|---|---|
| chirp 3,200 (comb reference) | 10/10 | 5,070-5,484 | 32,758 | - |
| hop5 25x5 gap 10 | 10/10 | 3,433-3,584 | 32,464-32,536 | 182-184 ms |
| hop4 30x4 gap 10 bin-aligned | 10/10 | 3,366-3,679 | 32,679-32,693 | - |

No failures. The plain chirp was 0/10 here in the morning (3,200 Hz in a null, 24-25 ms seen); in the afternoon 3,200 Hz is strong (single-tone capture 36,900): the comb moved within the day.
