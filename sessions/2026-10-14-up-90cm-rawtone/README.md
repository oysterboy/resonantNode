# 2026-10-14 step 5a (#35): raw received tone vs frequency, 90 cm, up, Emitter 0.5 FS

Same setup and tool as 2026-10-14-up-70cm-rawtone (rawtone.py, 2 reps), boards at 90 cm.

| f (Hz) | 90 cm | 70 cm |
|---|---|---|
| 3000 | 38,342 / 38,286 | 36,233 / 36,478 |
| 3100 | 9,523 / 10,390 | 14,780 / 14,200 |
| 3200 | 49,084 / 49,629 | 18,085 / 17,847 |
| 3300 | 60,045 / 65,761 | 54,460 / 54,449 |
| 3400 | 34,711 / 34,379 | 32,772 / 30,820 |

3200 Hz is 2.7x stronger at 90 cm than at 70 cm, matching the T3 results (90 cm
10/10, 70 cm 0/10 at 3200 Hz). 3100 Hz is weak at both spacings and 3300 Hz is
strongest at both; 3000 and 3400 Hz hardly change. So the pattern over frequency
is not the same at every spacing (3200 moves), but part of it is stable here.
Two spacings, one room, one board pair: not a general rule.
