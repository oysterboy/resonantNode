# 2026-10-15 5b: amp inspector medium limit 1,500 (build override; default 2,500): false answers

E2 Analyzer COM5 built with `-D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f` (experimental; the override is in
`DetectionProfile.h`, default unchanged), E3 Emitter idle, 110 cm, boards up. Radio 50 cm from E2, full volume, 90 deg to the mic.
`SEQ OBS ... freq=3300 band=2800-3800`, default scalar params, 50 windows each. Companion: 2026-10-15-ampthr1500-110cm (hop cells).

| Noise | amp medium 2,500 (2026-10-15-band-noise) | amp medium 1,500 |
|---|---|---|
| speech (WDR5) | 0 accepted, 0 valid | 2 accepted, 2 valid |
| music (drum and bass) | 0 accepted, 0 valid | **18 accepted, 18 valid** (36 %) |

Lowering the amp limit rescues the 110 cm hop cells (2026-10-15-ampthr1500-110cm: 5-step hop gap 5 ms 0/10 -> 10/10, gap 15 ms with max_duration 300
6/10 -> 10/10, plain chirp unchanged 10/10) but lets music through, so it is not a usable fix. Single runs; the 2,500 reference was
taken at 90 cm.
