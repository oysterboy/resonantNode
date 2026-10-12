# 2026-10-12 step 5 (#8) at 110 cm: INVALID, Emitter amp dead

E3 (Emitter, COM4) -> E2 (Analyzer 261f088, COM3), 110 cm. T3_scalar and
T2_freq: 50 miss of 50 each. Not detection data: the SELFTEST right after
(sessions/2026-10-12-selftest-step5-110cm-check) shows E3 `amp FAIL`, tone
2,895 vs ~460k before (-48.9 dB, DEGRADED), while E2 mic/amp PASS. Same
pattern as the 2026-10-11 soak-gate (E3 amp VIN/DIN left loose after the
boards were handled). E3 had been in flash mode and moved before the run.
Re-seat E3's amp VIN/DIN/GND, run SELFTEST on E3 until amp PASS, then redo
both runs.
