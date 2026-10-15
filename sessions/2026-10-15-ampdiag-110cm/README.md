# 2026-10-15 5b: amp/contrast statistics per occurrence (SEQ DIAG on), 110 cm, boards up

E2 Analyzer COM5 experimental build (FEATURE_HISTORY_BINS=512, SCALAR_AMP_MEDIUM_THRESHOLD=1500, b22b035), E3 Emitter hop 0.5 FS. `SEQ_INSPECT` lines carry p75/p90/mean/peak of the amp and contrast inspectors.

| Signal | amp P75 | amp P90 | amp peak | contrast P75 |
|---|---|---|---|---|
| chirp 3200 | 3,812-4,520 | 4,010-4,758 | 4,346-5,182 | 32,756-32,758 |
| hop5 gap 5 | 2,282-2,363 | 2,693-2,787 | 4,668-4,986 | 31,110-31,415 |
| hop5 gap 15 (max_dur 300) | 1,931-2,630 | 2,394-3,812 | 2,765-6,405 | 31,155-32,245 |
| radio music (15 inspected of 50 windows) | 2,473-4,473 | 2,720-5,845 | 3,376-7,766 | 25,654-31,223 (1 of 15 >= 30,500) |

No amp statistic separates hops from music; contrast P75 does, except for one music occurrence (31,223). Radio content differs from run to run (same type, volume, orientation only): single runs are samples. 14/15 music occurrences are above the 2,500 amp limit today, against 0 valid false answers in the earlier 2,500 reference run, so that reference was luck of the programme. Needs: 2 more music and 2 speech runs before a threshold is fixed.
