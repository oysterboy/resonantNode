# 2026-10-14 step 5 (#8): music (drum and bass), Scalar vs Freq, post-R2 (main c34fee9), old geometry

E2 Analyzer, E3 Emitter idle, drum and bass from the radio, 50 cm from the
Analyzer, full volume, speaker 90 deg to the mic. 50 silent OBS windows per
profile (period 2000, window 1800, report_settle 200).

| Profile | Detector accepted | Pattern-valid (false alarms) | Source rejects | Pattern rejects |
|---|---|---|---|---|
| Scalar | 0 | 0 | 49 (45 duration_too_short, 4 coverage_too_low) | 0 |
| Freq | 7 | 1 (the "late" window) | 20 (duration_too_short) | 6 |

2026-10-13 (before R2, other music): Scalar 1/50 valid, Freq 0/50 valid with
14 detector accepts rejected later. The music differs between the two days
(this one is drum and bass), so the counts are not directly comparable.
Room conditions not recorded beyond the setup above.
