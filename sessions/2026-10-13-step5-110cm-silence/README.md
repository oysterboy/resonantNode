# 2026-10-13 step 5 (#8): silence at the 110 cm setup, Scalar vs Freq

Analyzer E2 (COM3, 7882627), Emitter E3 (COM4) held silent with
EMIT MODE REMOTE, boards at the 110 cm setup of
sessions/2026-10-12-step5-110cm-b, room silent (owner). 50 observation
windows per profile (SEQ OBS, period 2000, window 1800), stock profiles.

| Profile | windows | accepted (false accepts) | other |
|---|---|---|---|
| TonalPulseScalar | 50 | 0 | 1 rejected: trial 35, a 1 ms transient, source_strength 2,405, reject duration_too_short |
| TonalPulseFreq | 50 | 0 | 1 `unexpected`: trial 13, src_total=0, strength 0, `missing_pipeline_result` (no source, not a detection) |

Both profiles: 0 false accepts in 50 silent windows. Scalar sees a click and
rejects it; Freq's one flagged window had no candidate at all, so it looks
like a runner/pipeline artefact, not a sound (not investigated).
Speech and music were not run in this session (R4, #20: Scalar answered
speech 2/50 and music 6/50; Freq was never measured against them).

Before this run, a `soak.py` left running on COM3 (started 2026-10-12
20:13, resuming sessions/2026-10-12-soak24h-70cm, blocks up to 83) was
stopped by the owner's request. Its blocks after the boards were moved
to 110 cm (strength median ~6.4k instead of 9-11k) sit in the 70 cm
session; treat that session's data after the move as 110 cm data.
