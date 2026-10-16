# 2026-10-16 5b gate battery, 110 cm, combined build (tools/bench/gate5b.sh)

E2 Analyzer COM5 with -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f (162c847 + overrides), E3 Emitter hop 0.5 FS, mic and speaker facing up, 110 cm. SEQ DIAG on. 10 trials per cell.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| plain chirp 3,200 Hz | 10/10 | 2,042-2,427 (2026-10-15: 3,812-4,520) | 32,743-32,749 |
| 3-step hop 30 ms x3, gap 10 | 10/10 | 3,148-3,296 | 32,689-32,705 |
| 5-step hop 20 ms x5, gap 5 | 10/10 | 2,652-2,829 | 31,491-31,922 |
| 5-step hop 20 ms x5, gap 15 (max_duration 300) | 1/10 | 2,161-2,322 (3 inspected) | 29,924-32,402 (3 inspected) |

The gap-15 cell fails before the inspectors in 7 of 10 trials (pattern_rejected / coverage_too_low, detector sees 88-89 ms and 79-80 ms coverage) and contrast is weak (about 30,000) in 2 of the 3 that reached the inspection: with long gaps the contrast P75 over 100 ms is below the 30,500 limit. The chirp amp is half of 2026-10-15 at the same nominal spacing: the comb moved.
