# 2026-10-22: live detection params (step 10 item 2, D2)

Setup: trial layout; E2 (COM3) Node, E3 (COM4) Emitter 23d034a, 110 cm; E1/E4/E5 held silent. Script:
midtrain.py (boot, wait 12 s, then 10 times: `EMIT PULSES` hop train on E3, wait N s, `PARAM SET
detection.scalar_onset_threshold 2000` (the shipped value, so only the apply path differs) on E2, wait 6 s; then
RB summary).

| firmware | PARAM SET after the command | heard (accepted / valid) |
|---|---|---|
| b0c8fba (before: PARAM SET resets the detector and FeatureHistory) | 100 ms | 10 / 10 |
| b0c8fba | 180 ms | **0 / 0** |
| 7c2c28e (live apply) | 100 ms | 10 / 10 |
| 7c2c28e | 180 ms | **10 / 10** |

At 100 ms (about 50 ms into the 165 ms train) the old reset still left enough of the train for a new candidate.
At 180 ms (about 130 ms in) it cannot recover, and the old firmware loses every train.
