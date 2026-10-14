# 2026-10-14 step 5a (#35): spacing map, mic + speaker facing up, Emitter 0.5 FS, Scalar

Index of the 10-trial T3 runs (E2 Analyzer, E3 Emitter -D I2S_TONE_AMPLITUDE=0.5f,
main c34fee9, window and doors closed, Roomba off; first 70 cm runs were noisier,
see their README). No data lives in this folder.

| Spacing | Session | Accepted | Strength | Duration | Amp class |
|---|---|---|---|---|---|
| 60 cm | 2026-10-14-up-60cm-0p5 | 0/10 | ~3,400 | 91-100 (3 of 10: 44) | weak / unknown |
| 70 cm | 2026-10-14-up-70cm-0p5 (f10) | 0/10 | 5,800-6,200 | 50-74 (2 of 10: ~125) | unknown |
| 80 cm | 2026-10-14-up-80cm-0p5 | 0/10 | ~4,500 | 82-112 | weak / unknown |
| 90 cm | 2026-10-14-up-90cm-0p5 | 10/10 | 7,600-8,100 | 118-127 | medium |
| 100 cm | 2026-10-14-up-100cm-0p5 | 0/10 | ~3,700 | ~45 | unknown |
| 110 cm | 2026-10-14-up-110cm-0p5 (50 trials), -recheck (10) | 50/50, 10/10 | 6,965 avg; 5,900-6,600 | 100-159 | medium |

Strength changes by a factor of two between neighbouring 10 cm steps and does not follow
distance: with speaker and mic facing up the level at one tone frequency depends on the
exact spacing (reflections / standing wave; the wavelength of 3,200 Hz is ~10.7 cm). The
110 cm result reproduces; 90 cm passes; 60-100 cm otherwise fails at 0.5 FS.
At 70 cm, 0.3 FS was worse (0/50, 38-67 ms): 2026-10-14-up-70cm-0p3.

## 3,300 Hz (SEQ freq=3300), same setup, 10 trials each

Sessions 2026-10-14-up-<N>cm-0p5-f3300 (T3_scalar_f3300_10).

| Spacing | 3,200 Hz | 3,300 Hz | 3,300 Hz strength / class |
|---|---|---|---|
| 60 cm | 0/10 | 10/10 | 9,500-10,150, strong |
| 70 cm | 0/10 | 10/10 | 5,900-6,600, medium |
| 80 cm | 0/10 | 8/10 (2 weak) | ~8,700, medium / weak |
| 90 cm | 10/10 | 10/10 | 11,700-14,700, strong |
| 100 cm | 0/10 | 0/10 | ~6,200, weak |
| 110 cm | 50/50, 10/10 | 10/10 | ~6,350, medium |

Neither frequency passes everywhere. Class boundaries decide the borderline cases
(100 cm at 3,300 Hz has the strength of 70 cm but the amp class comes out weak).
Raw tone amplitudes at 70 and 90 cm: 2026-10-14-up-70cm-rawtone, -up-90cm-rawtone.
Interpretation so far (not proven): mostly multipath because the speaker beam points
up, plus a smaller fixed hardware response (3,100 Hz weak and 3,300 Hz strong at both
raw-scanned spacings).

## Raw tone amplitude vs frequency, 100 and 90 cm (2026-10-14-up-100cm-rawscan, -up-90cm-rawscan)

| f (Hz) | 100 cm | 90 cm |
|---|---|---|
| 2800 | 60,457 | 38,655 |
| 2850 | 19,494 | 43,934 |
| 2900 | 48,674 | 42,772 |
| 2950 | 60,358 | 33,415 |
| 3000 | 71,565 | 31,828 |
| 3050 | 46,932 | 7,306 |
| 3100 | 28,222 | 22,397 |
| 3150 | 12,359 | 63,736 |
| 3200 | 11,065 | 59,735 |
| 3250 | 17,945 | 44,706 |
| 3300 | 35,276 | 69,620 |
| 3350 | 22,306 | 72,685 |
| 3400 | 3,632 | 36,279 |
| 3450 | 68,270 | 77,909 |
| 3500 | 71,454 | 75,345 |
| 3550 | 11,941 | 16,946 |
| 3600 | 36,052 | 58,636 |

Peaks and nulls are 20x deep and a few hundred Hz apart at each spacing. Most of
them move between 90 and 100 cm (3,050 null at 90 only; 3,150-3,200 weak at 100 and
strong at 90; 3,400 null at 100 only). A few features stay: 3,450-3,500 strong and
3,550 weak at both. Correlation of log amplitude between the two spacings: see the
session log in the commit message / chat. Reading: mostly spacing-dependent
interference with a small fixed part; consistent with the detector results.
