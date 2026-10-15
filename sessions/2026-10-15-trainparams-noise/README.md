# 2026-10-15 DET-011: false answers with the pulse-train params vs defaults (512-bin history, 3,300 Hz target)

E2 Analyzer COM5 (512-bin FeatureHistory build, 22fdba4), E3 Emitter idle, boards facing up, 90 cm. Radio 50 cm from E2, full
volume, 90 deg to the mic: WDR5 speech, then drum and bass. 50 OBS windows each (period 2000, window 1800, settle 200),
`freq=3300`. Train params = max_gap 130, max_duration 600, min_release_coverage 40, release_debounce 130 (what merges
15 ms x3 trains across 60 ms gaps, 2026-10-15-pulses-hist512). Default = stock scalar params.

| Noise | Params | Detector accepted | Valid (false answers) | Source rejects |
|---|---|---|---|---|
| speech | train | 0 | 0 | 32 |
| speech | default | 1 | 1 | 34 |
| music | train | 0 | 0 | 32 |
| music | default | 0 | 0 | 39 |

The permissive parameters add no false answers on speech or music here (one window of difference on speech, in the
other direction). One run per cell, one radio program each, 90 cm.
