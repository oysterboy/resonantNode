# 2026-10-14 step 5a (#35): raw received tone vs frequency, 70 cm, up, Emitter 0.5 FS

E2 Analyzer, E3 Emitter (0.5 FS), 70 cm, facing up, window and doors closed,
Roomba off. `tools/bench/rawtone.py`: RAW trigger f=.. dur=100 mode=i2s, tone
amplitude = single-bin DFT of the raw samples over the emit window. The detector
is not involved. Raw CSVs are in this folder.

| f (Hz) | amplitude (2 reps) |
|---|---|
| 3000 | 36,233 / 36,478 |
| 3100 | 14,780 / 14,200 |
| 3200 | 18,085 / 17,847 |
| 3300 | 54,460 / 54,449 |
| 3400 | 32,772 / 30,820 |

Repeats agree within ~5 %; neighbouring frequencies differ by up to 3.8x (3100
vs 3300 Hz). At 70 cm the received level depends strongly on the tone
frequency: 3200 Hz sits near a low point. Consistent with a reflection /
standing-wave pattern; not yet compared with a passing spacing (90 cm).
