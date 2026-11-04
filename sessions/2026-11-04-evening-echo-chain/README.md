# 2026-11-04 evening: the echo room on all five, the chain with default vs spread answers (unattended)

All five on esp32dev-echo a7d6715c, echo.gain 64 (not 128: no powered hub yet; no port dropped). The lab PC (near
E3) played claps_sparse.wav: four synthetic claps 25 s apart, per section. deskrun.py plan.json.

| setting | replays | chains (3 s gap) | longest |
|---|---|---|---|
| default (wait 350, jitter 300, level_max 4, offset 2, rest 4000, attack 20, pre 30 / post 270, onset 1000, peak 1200) | 18 | 4: 5, 5, 4, 4 replays | 0.7 s: the boards answered together, one round |
| spread (wait 700, jitter 1500, level_max 20, offset 20, rest 2000, attack 40, pre 100 / post 200, onset 800, peak 800) | 59 | 9: 13, 16, 1, 11, 1, 2, 9, 2, 4 | 10.5 s, 5.9 s, 5.7 s |

The spread carries the chain: answers spaced over 0.7-2.2 s let the others hear them. Claps heard while an answer
was already pending: 47 (already_scheduled).
