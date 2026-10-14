# 2026-10-14 step 5 (#8): speech, Scalar vs Freq, post-R2 (main c34fee9), old geometry

E2 Analyzer, E3 Emitter idle, radio WDR5 50 cm from the Analyzer, full
volume, speaker 90 deg to the mic (as 2026-10-13-step5-speech). 50 silent
OBS windows per profile (period 2000, window 1800, report_settle 200).

| Profile | Detector accepted | Pattern-valid (false alarms) | Source rejects |
|---|---|---|---|
| Scalar | 0 | 0 | 26 (all duration_too_short) |
| Freq | 0 | 0 | 14 (all duration_too_short) |

Room conditions not recorded beyond the setup above. 2026-10-13 (before R2):
0 valid for both.
