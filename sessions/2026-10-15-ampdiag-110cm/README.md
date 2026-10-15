# 2026-10-15 5b: amp/contrast statistics per occurrence (SEQ DIAG on), 110 cm, boards up

E2 Analyzer COM5 experimental build (FEATURE_HISTORY_BINS=512, SCALAR_AMP_MEDIUM_THRESHOLD=1500, b22b035), E3 Emitter hop 0.5 FS. `SEQ_INSPECT` lines carry p75/p90/mean/peak of the amp and contrast inspectors.

| Signal | amp P75 | amp P90 | amp peak | contrast P75 |
|---|---|---|---|---|
| chirp 3200 | 3,800-4,500 | 4,000-4,700 | 4,300-4,800 | 32,757 |
| hop5 gap 5 | 2,300-2,360 | 2,700-2,770 | 4,800-5,000 | ~31,200 |
| hop5 gap 15 (max_dur 300) | 2,400-2,630 | 3,700-3,800 | 6,100-6,400 | ~31,350 |
| radio music (15 inspected of 50 windows) | 2,470-4,470 | 2,720-5,840 | 3,380-7,770 | 25,650-31,220 (14/15 < 29,500) |

No amp statistic separates hops from music; contrast P75 does, except for one music occurrence (31,223). Radio content differs from run to run (same type, volume, orientation only): single runs are samples. 13/15 music occurrences are above the 2,500 amp limit today, against 0 valid false answers in the earlier 2,500 reference run, so that reference was luck of the programme. Needs: 2 more music and 2 speech runs before a threshold is fixed.
