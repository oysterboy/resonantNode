# 2026-10-16 5b: radio speech (WDR5) and drum and bass (Spotify) from the phone, 2 runs of 50 windows each

E2 Analyzer COM5 experimental build (b22b035: amp medium 1500, contrast medium 20000 default, FEATURE_HISTORY_BINS 512), E3 Emitter idle, 110 cm, boards up, phone 50 cm from E2, 90 deg, full volume, as 2026-10-15. `SEQ DIAG on`, band 2800-3800, default scalar params. Radio content differs from run to run.

| Run | detector candidates | reached inspection (valid false answers) | amp P75 | contrast P75 |
|---|---|---|---|---|
| speech r1 | 37 | 0 | | |
| speech r2 | 45 | 3 (2 late, 1 expected) | 4,162-4,469 | 27,954-30,092 |
| music r1 | 41 | 0 | | |
| music r2 | 34 | 9 (late) | 4,119-4,821 | 25,565-27,701 |

All 12 valid false answers have amp P75 well above 2,500, so no amp limit between 1,500 and 4,000 stops them; contrast P75 is below 30,500 for all 12 (max 30,092). Pooled with 2026-10-15 music (15 occurrences, 1 at 31,223): the hops (31,110-32,245) and these false answers are separated at about 30,500 except for that one occurrence. Single pass per source.
