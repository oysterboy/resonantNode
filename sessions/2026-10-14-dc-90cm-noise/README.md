# 2026-10-14 step 5a item 2b (#35): DC blocker, silence / speech / music at 90 cm (code bfbe0c8)

E2 Analyzer default build (DC blocker 100 Hz, output x0.588), E3 Emitter 0.5 FS idle, boards
facing up, 90 cm, window and doors closed, Roomba off. Radio 50 cm from E2, full volume, 90 deg
to the mic: WDR5 speech, then drum and bass. 50 OBS windows each (period 2000, window 1800,
report_settle 200), Scalar.

| Run | Detector accepted | Pattern-valid (false alarms) | Source rejects |
|---|---|---|---|
| OBS_scalar_silence | 0 | 0 | 0 |
| OBS_scalar_speech | 0 | 0 | 11 (10 duration_too_short, 1 coverage_too_low) |
| OBS_scalar_music | 0 | 0 | 43 (duration_too_short) |

First Difference at 110 cm (0.5 FS, facing up, 2026-10-14-up-110cm-0p5): speech 20 source
rejects, music 40. The higher mic floor of the DC blocker (SELFTEST floor ~4x) does not show
as false alarms here. Node build not yet run with the new filter.
