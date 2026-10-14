# 2026-10-14 step 5 (#8): Scalar vs Freq at 110 cm after the tier R2 fixes (main c34fee9)

E2 Analyzer (COM3), E3 Emitter (COM4), 110 cm, as 2026-10-13-step5-110cm-b.
SELFTEST gate passed first (2026-10-14-selftest-step5-r2-gate).

| Run | Result |
|---|---|
| T3_scalar (50) | 33 expected, 17 rejected in one block (trials 8-24: 13 duration_too_short at 52-84 ms, 4 coverage_too_low); no room notes |
| T3_scalar_b (50) | 50 expected |
| T3_freq (50) | 50 expected, avg strength 8,763 |
| OBS_scalar_silence (50 windows) | 0 detector accepts |
| OBS_freq_silence (50 windows) | 0 detector accepts |

Speech and music not run (owner left). Freq durationMs runs ~50 ms over the
true tone (see #32 comment), Scalar ~10 ms.
