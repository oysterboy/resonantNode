# 2026-10-15 corrected Analyzer health thresholds + new pulse-train Emitter, plain T3

E2 Analyzer (COM5) with the loud-end diagnostics limits scaled x256 (`AnalyzerModeApp.cpp`: large
jump, RMS too high, raw clip), E3 Emitter (COM3) 0.5 FS with PULSES (main ce586c1), 90 cm, up.
T3_f3300_10: 10/10 expected, `RAW_AUDIO_HEALTH verdict=ok` in all 10 windows (before the fix: clipped
in all of them at the same placement and level). The new Emitter's plain chirp path is unchanged.
