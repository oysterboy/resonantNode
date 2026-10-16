# 2026-10-16 5b gate, noise side, combined build: radio speech (WDR5) and drum and bass from the phone, 2 x 50 windows each

E2 Analyzer COM5 combined build (162c847 + -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f), Emitter idle, phone 50 cm from E2, 90 deg, full volume (as 2026-10-16-radio-phone, which used the 1500/20000 build). Radio content differs from run to run.

| Run | detector candidates | reached inspection | contrast P75 | valid false answers |
|---|---|---|---|---|
| speech r1 | 40 | 2 | 20,068-28,764 (weak) | 0 |
| speech r2 | 45 | 3 | 25,025-29,543 (weak) | 0 |
| music r1 | 44 | 0 | - | 0 |
| music r2 | 43 | 0 | - | 0 |

Speech: 5 occurrences reached the inspectors, all rejected on contrast (the amp class was medium or strong, so the amp limit would not have stopped them). Same setup earlier today with the 1500/20000 build: 3 valid false answers in 100 speech windows, 9 in 100 music windows. Music today: nothing reached inspection, so the contrast gate was not exercised by music in this session. Single passes.
