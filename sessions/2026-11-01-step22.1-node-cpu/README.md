# 2026-11-01: step 22.1, the Node's per-sample cost at band update 4 and 16

E5 by `NET E5 OTA` from E1: the Node image b4f1a7c6 (RB STATUS audio.us_per_sample / audio.block_us_max, new) at
FREQBAND_UPDATE_EVERY_SAMPLES 4 (the default) and the same commit built with 16, then back to 4. E5 silent (stored),
8-bin band, base 1. Per image: 3 windows of 60 s quiet, 2 windows of 60 s with the Emitter's hop10 every 4 s.
node_cpu.log.

| update every | us per sample (quiet / hop trains) | share of the 62.5 us a sample allows | longest block | dropped DMA buffers |
|---|---|---|---|---|
| 4 | 34.60 / 34.47-34.57 | 55 % | 4.45 ms / 5.76-5.98 ms | 0 |
| 16 | 15.70 / 15.71-15.86 | 25 % | 2.03 ms / 3.34-3.51 ms | 0 |

The bank at 4 costs ~19 us a sample on the Node (the Analyzer's profile: ~27 us with its micros() calls); at 16 the
whole per-sample path takes a quarter of the budget. The longest block halves.
