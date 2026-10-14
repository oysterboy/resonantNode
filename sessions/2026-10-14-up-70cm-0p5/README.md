# 2026-10-14 step 5a (#35): upwards geometry, 70 cm, Emitter tone 0.5 FS (Scalar, main c34fee9)

E2 Analyzer, E3 Emitter (-D I2S_TONE_AMPLITUDE=0.5f), both facing up, 70 cm.
Window open at first (kids playing, an aircraft overhead); closed from
T3_scalar_d10 on. SELFTEST (2026-10-14-selftest-up-70cm-0p5-gate): both PASS,
E2 floor 1639 (110 cm: 905), snr 42.8 dB.

| Run | Window | Result |
|---|---|---|
| T3_scalar (50) | open | 14 expected, 36 rejected (31 evaluator pattern_rejected with amp class weak, 5 source duration_too_short); strength 4,380-7,906 |
| OBS_scalar_silence (50) | open | 0 detector accepts |
| T3_scalar_b (50) | open | 0 expected, 50 rejected, all source duration_too_short at 41-46 ms, strength ~4,900 |
| T3_scalar_c10 (10) | open | 0 expected, durations 43-46 ms, strength ~4,800 |
| T3_scalar_d10 (10) | closed | 0 expected, durations 55-58 ms (one 129), strength 7,300-7,800, one island, no gap |

Worse than 110 cm at the same level (50/50, 100 ms). The detector sees one
clean, short event (island = coverage, gap 0), not fragments, so the received
tone is shorter or cut at ~45-57 ms. Cause not found: no distance sweep
conclusion yet. Speech and music not run at 70 cm.
