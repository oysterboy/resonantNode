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
