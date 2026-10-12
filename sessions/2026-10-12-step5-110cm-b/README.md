# 2026-10-12 step 5 (#8) at 110 cm: Scalar vs Freq, stock profiles

E3 Emitter (COM4) -> E2 Analyzer (COM3, 7882627), 110 cm, speaker faces
mic, 0.3 FS 3200 Hz, 50 tries each, mode=detail. Redo of
sessions/2026-10-12-step5-110cm (invalid: E3 amp dead). E3 SELFTEST amp
PASS 60.8 dB right before (sessions/2026-10-12-selftest-e3-recheck2). The
owner removed some Dupont extension leads before this run: the earlier
amp failures on E3 (and the 2026-10-11 loose VIN) were most likely contact
faults in those leads.

| Profile | expected | rejected/miss/late/early | strength min / med / max | avg dt |
|---|---|---|---|---|
| TonalPulseScalar | 50/50 | 0 | 8,322 / 8,992 / 10,400 | 26 ms |
| TonalPulseFreq | 50/50 | 0 | 6,272 / 8,690 / 9,151 | 24 ms |

Both profiles detect every chirp at 110 cm on this pair; no false results
in these runs (silent windows were not run). This differs from the 2026-10-09
overnight record at 110 cm (Freq 36-40/50, DET-010), which was taken on
other firmware and wiring; see damp-bench-check.md. The profile choice
(#8) now turns on silence behaviour (Scalar answered speech 2/50 and music
6/50, R4), not on detection at this distance. Freq's strength spread is
wider (min 6.3k vs 8.3k).
