# 2026-10-14 DET-012: speech and music false alarms at 2,000 and 1,500 Hz (DC blocker, 90 cm, up, 0.5 FS, Scalar)

E2 Analyzer default build (DC blocker, code bfbe0c8), E3 Emitter idle (REMOTE), boards facing up,
90 cm. Radio 50 cm from E2, full volume, 90 deg to the mic: WDR5 speech, then drum and bass.
50 silent OBS windows per run (period 2000, window 1800, report_settle 200), `freq=` sets the band
target. Silence at both frequencies and the chirp runs: 2026-10-14-lowfreq-90cm.

| Frequency | Noise | Detector accepted | Pattern-valid (false answers) |
|---|---|---|---|
| 3,200 Hz (2026-10-14-dc-90cm-noise) | speech | 0 | 0 |
| 3,200 Hz | music | 0 | 0 |
| 2,000 Hz | speech | 3 | 3 |
| 2,000 Hz | music | 4 | 4 |
| 1,500 Hz | speech | 2 | 1 |
| 1,500 Hz | music | 35 | **30** |

Speech and music have far more energy that passes the Scalar chirp definition at lower
frequencies; at 1,500 Hz drum and bass is accepted as a chirp in 30 of 50 windows. Detection of
the real chirp works at those frequencies (2026-10-14-lowfreq-90cm) but noise rejection does
not carry over. One room, one placement, one radio program per type, no room notes.
