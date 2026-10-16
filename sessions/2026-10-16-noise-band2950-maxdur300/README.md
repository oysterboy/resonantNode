# 2026-10-16 noise side with the shipped values: band 2950-3950, max duration 300

The one combination that had not run on the board: the band plus `scalar_max_duration_ms=300` (the afternoon noise runs
used the default 200). E2 Analyzer COM5, combined build 162c847 + overrides (= the defaults shipped in c26b35d), E3
silent (OBS), boards at 70 cm, phone radio 50 cm from the E2 mic at 90 degrees, full volume, as on 2026-10-15/16.
`SEQ OBS start tries=50 period=2000 window=1800 report_settle=200 band=2950-3950`, SEQ DIAG on.

| Run | windows | detector candidates (rejected) | reached inspection | contrast P75 of those | valid false |
|---|---|---|---|---|---|
| OBS_speech (WDR5) | 50 | 38 | 2 | 27,902-28,191 (weak) | 0 |
| OBS_music (drum and bass) | 50 | 48 | 16 | 25,622-30,085 (all weak) | 0 |

Music exercised the contrast gate this time (16 inspected, against 0 in the afternoon's combined-build run); the
highest value, 30,085, sits 415 below the 30,500 limit, the same thin margin as the 2026-10-15 outlier (31,223)
suggests. Amp P75 of the inspected occurrences 3,274-5,094: all above the 1,500 amp limit, so the contrast gate
alone keeps them out, as the decision says.
