# 2026-10-14 step 5a (#35): upwards geometry, 110 cm, Emitter tone 0.5 FS (Scalar, main c34fee9)

E2 Analyzer (stock), E3 Emitter built with `-D I2S_TONE_AMPLITUDE=0.5f`; both
boards facing up on the bench, 110 cm between them. Reference at 0.3 FS:
2026-10-14-up-110cm-operating-point (27/50).

SELFTEST (2026-10-14-selftest-up-110cm-0p5-gate): both PASS, E3 own tone
+4.4 dB vs 0.3 FS (366k vs 220k): the amp is linear in this range.

| Run | Result |
|---|---|
| T3_scalar (50) | 50 expected; contrast medium, amp medium on every trial; avg strength 6,965 (0.3 FS: 4,190) |
| OBS_scalar_silence (50) | 0 detector accepts |
| OBS_scalar_speech (50, WDR5, 50 cm, full volume, speaker 90 deg to the mic) | 0 accepts, 0 valid; 20 source rejects (duration_too_short) |
| OBS_scalar_music (50, drum and bass, same setup) | 0 accepts, 0 valid; 40 source rejects (38 duration_too_short, 2 coverage_too_low) |

No room notes. Not done at 0.5 FS: 70 cm, 40 cm, Node build.
