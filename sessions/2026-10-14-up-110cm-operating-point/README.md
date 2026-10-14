# 2026-10-14 step 5a (#35): operating point, mic + speaker facing up, 110 cm (Scalar, main c34fee9)

E2 Analyzer, E3 Emitter, both on the bench facing up, 110 cm between them,
flat surface. SELFTEST first (2026-10-14-selftest-up-110cm-gate): both PASS,
own tone -5.6 dB (E2) and -11.3 dB (E3, flagged DEGRADED by the runner) vs the
horizontal setup; expected, the speaker no longer faces the mic.

| Run | Result |
|---|---|
| T3_scalar (50) | 27 expected, 23 rejected (all stage=evaluator pattern_rejected); avg strength 4,190 (horizontal: ~8,300) |
| OBS_scalar_silence (50 windows) | 0 detector accepts, 0 pattern-valid |

Rejects vs accepts: same contrast class (medium) and strength range (rejected
4,034-4,269, expected 4,113-4,398); the difference is the amp class, medium
(27) vs weak (23). The Scalar amp inspector's medium threshold sits inside the
spread of the new, weaker signal. No thresholds changed.
