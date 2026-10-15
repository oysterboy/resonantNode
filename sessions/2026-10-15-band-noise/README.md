# 2026-10-15 DET-013: false answers with the band bank (band=2800-3800), speech and music

E2 Analyzer COM5 with the 512-bin history and the band bank (firmware banner b309e96; the band code is b22b035, committed
right after the flash), E3 Emitter idle, boards facing up, 90 cm, `freq=3300`. Radio 50 cm from E2, full volume, 90 deg to the
mic: WDR5 speech, then drum and bass (one ad break interrupted the first music attempt; the runs here are complete 50-window
runs). 50 OBS windows each (period 2000, window 1800, settle 200). Train params = max_gap 130, max_duration 600,
min_release_coverage 40, release_debounce 130.

| Noise | Params | Detector accepted | Valid (false answers) |
|---|---|---|---|
| speech | band, default | 0 | 0 |
| speech | band, train params | 4 | **4** |
| music | band, default | 0 | 0 |
| music | band, train params | 0 | 0 |

Reference, single 3,300 Hz bin (2026-10-15-trainparams-noise): speech default 1 / train 0; music 0 / 0. The band alone adds no
false answers; band plus the permissive train params adds 4 on speech (8 % of windows), not on music. One run per cell.
