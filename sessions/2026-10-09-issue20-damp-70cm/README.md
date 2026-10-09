# 2026-10-09 issue #20: D-AMP at 70 cm

Setup: D-AMP Emitter COM6 -> D-AMP Analyzer COM10, 70 cm, speaker faces mic,
same height. Firmware 0566cc1 (board=damp), tone 0.1 FS, 5 ms ramp. Compare
with piezo bench:sessions/2026-10-08-issue7-70cm (same distance; T3 48/50).

| Run | Result |
|---|---|
| T3_scalar (TonalPulseScalar, 50) | 46 expected, 4 rejected; detector accepted 50/50; avg strength 4636 |
| T2_freq (TonalPulseFreq, 50) | 50 miss (profile defaults never fire; same on piezo) |

The 4 rejected were the weakest trials (strength 3664-4067 vs median 4700),
dropped by the contrast verdict requirement, not by the detector.
