# 2026-10-09 issue #20: D-AMP at 200 cm

Setup: as the 70 cm session, boards moved to 200 cm. Firmware 0566cc1.

| Run | Result |
|---|---|
| T3_scalar (0.1 FS) | 0/50: 35 miss, 15 rejected (onsets peaking ~2000, under min peak 3000) |
| T3_scalar_amp040 (Emitter 943ec31 -D I2S_TONE_AMPLITUDE=0.4f) | 50 miss, detector never fired |

**T3_scalar_amp040 is invalid.** Later the same evening the COM6 amp/speaker
was found silent at every level, including 0.1 FS (bench:sessions/
2026-10-09-issue20-damp-110cm-b README). The owner moved the boards around
this run; the fault most likely started then. Whether 0.4 FS works on this
amp is untested.
