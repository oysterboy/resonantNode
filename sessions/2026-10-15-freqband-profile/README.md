# 2026-10-15 DET-013 step 1: profile of the band stream on D-AMP (main ce586c1)

E2 on COM5 (the ports moved: E3 is COM3 today), Analyzer built with
`-D FREQBAND_ENABLE_PROFILING=1`, E3 Emitter (0.5 FS, older build) chirping as load. Placement
after the re-plug is unknown; the T3 is only a load (10/10 expected). `SEQ start ... mode=system`,
then `SEQ REPORT` in the same serial session (opening the port resets the board; the profile line
exists only in that report). Files: seq_report.log, system_run.log.

FREQBAND profile (10 trials, 538,624 samples):

| item | value |
|---|---|
| compute calls | 134,641 (one per 4 samples, `updateEverySamples=4`) |
| avg compute | 58.85 us |
| avg energy pass | 6.55 us |
| avg Goertzel passes (target + 2 neighbours) | 24.67 us (~8 us per bin) |
| rest of compute (ring copy, packaging) | ~27 us |
| per sample | 58.85 / 4 = 14.7 us, i.e. 23.5 % of the 62.5 us sample period |
| SYSTEM_HEALTH loop_avg / update_loop_avg | 221 / 189 us |
| processed_ratio | 0.997-1.000; max_processing_lag 13 ms |

Reading: a bank of K = 8 bins at the same update rate would add ~5 x 8 = 40 us, i.e. compute ~100 us per
4 samples = 25 us per sample (40 % of the sample period) before the planned single-pass rewrite; the
rewrite should roughly halve the Goertzel and the ring-copy part. Updating every 8 samples halves it
again. No dropped buffers in these runs.

Side finding: `RAW_AUDIO_HEALTH verdict=clipped` on every trial window at this placement with the
Emitter at 0.5 FS, so the mic ADC clips during the chirp. That fits the saturated strength (32,767) and
`duration_too_long` at 800/1000 Hz on 2026-10-14 and deserves its own look (tone level vs. placement).
