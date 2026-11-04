# 2026-11-02/04: five Nodes free for 41 h, a band per node on 500-5000 Hz

All five boards as Nodes on 411fe034 (OTA). Live, unsaved (nodegroup --setup): output.silent 0, band_per_node 1,
base_scatter_bins 1, level_max 4, level_decay_ms 5000, answer_jitter_ms 800, idle announce 120 Hz / 80 ms, band
range 500-5000 (the param's floor is 500; the owner asked for 300). 2026-11-02 14:06 to 2026-11-04 07:25 (stopped by
the owner's word, 41.2 h), day and night, the desk. Summary: summary.txt (longeval.py).

- Health: no board reset (audio.uptime_ms continuous on all five), no dropped DMA buffer, heap free min steady at
  82 KB, block_us_max <= 3.5 ms; no serial port dropped (nodegroup's reopen never needed). The drops of 2026-11-01
  came with the echo room at gain 128: the amp's current, not the boards.
- Activity steady over the 41 h: ~480 emissions an hour for the five (178 idle calls, 299 answers), every 6 h block
  within 470-483.
- Chains (3 s gap): 6117, median 2 trains, max 9, the longest 8.2 s; none past 30 s; 1450 of 6+ trains.
- Valid verdicts ~39,000: 12,700 answered, 16,900 declined by the level draw, 8,300 in rest.
- The bases: the field drifted across the whole range: 1500-2000 Hz (positions 4-6) in the first 12 h, 2500-4000
  (8-13) from 12 to 24 h, split low (2-3) and high (12-13) from 24 to 36 h, spread at the end; the lowest position
  (500-1500) rarely. Pairs within two positions at the STATUS rounds: median 80 %, min 20 %.
