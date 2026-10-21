# 2026-10-22 (just after midnight): step 3d gate SEQ session on D-AMP (#31/#32)

Setup:
- E2 (COM3): Analyzer, faaa814 shipped.
- E3 (COM4): Emitter, faaa814 (0.5 FS default).
- E2 and E3 stand 110 cm apart in the trial layout; E1, E4 and E5 (Nodes) were held silent.
- The owner was away.

| check (#32) | result |
|---|---|
| P1 PARAM echo | `PARAM scalarMinStrength=1234 scalar_min_release_coverage_ms=90 scalar_min_longest_island_ms=80 scalarMinReleaseCoverageMs=95 scalar_max_gap_ms=10`, then `PARAM STATUS`: min_strength 1234.0, release_coverage 95 (the later alias of the same key wins), longest_island 80, max_gap 10. **PASS** |
| B12a | `SEQ start profile=Bogus mode=detail` prints `ERR SEQ unknown profile=Bogus ...` and starts nothing. **PASS**. `MODE` alone is not a command (`analyzer_unknown_cmd`), so "MODE unchanged" was checked as "no trial started". |
| B9 | `SEQ OBS start period=2000 window=1800` prints `OBS_WARN window+report_settle>period`; `period=2500 window=1800 report_settle=500` prints none. **PASS** (the 50-trial phase check was not run) |
| B12b | `SEQ start 5 sampleFirst=2`: two SAMPLES blocks at `sample_rate_ms=4`, no SAMPLES_WARN. **PASS** (rows not counted exactly) |
| P3 | T3 dur=40 with diag on: 9 rejected, 1 expected. Diag off: 10 rejected. Each trial is classified on its own, no PipelineQueueOverflow in any log, and the next normal T3 is 10/10 expected: nothing carried over. **PASS**. Not exercised: a miss (no occurrence) right after rejects; `freq=1000` still gave 5/5 expected, so `freq=` does not take the Emitter off the band alone. |

Not part of this session: A1, A3/A4, P2a/P2b, 12 (regression), 13 (B11), and P4 (largely superseded by the 2026-10-16
profile decision).
