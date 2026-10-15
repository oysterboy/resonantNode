# 2026-10-15 DET-013 step 3: band bank (`SEQ band=LOW-HIGH`), 90 cm, up, Emitter 0.5 FS

E2 Analyzer COM5 with the 512-bin FeatureHistory and the band bank in `FreqBandStream` (FrequencyTarget = strongest bin in the band;
bins one 250 Hz bin spacing apart plus a guard bin each side; contrast = peak bin vs the mean of its two neighbours, as the single
target's), E3 Emitter COM3 with hop. Band 2800-3800 Hz = bins 2800/3050/3300/3550/3800, guards 2550/4050. Train params:
max_gap 130, max_duration 600, min_release_coverage 40, release_debounce 130. 10 trials per cell.

| cell | result | amp class | dur / coverage / island / max gap (ms) |
|---|---|---|---|
| chirp 3,300 Hz, no band | 10/10 expected | strong | 164 / 160 / 118 / 3 |
| chirp 3,300 Hz, band 2800-3800 | 10/10 expected | strong | 165 / 160 / 117 / 3 |
| hop 2800,3300,3800 (15 ms x3, gap 20), band | **10/10 expected** | medium | 121 / 77 / 29 / 14 |
| hop 2800,3300,3800 (15 ms x3, gap 60), band | 0/10 pattern_rejected | weak | 202 / 81 / 29 / 58 |

The band bank leaves a chirp unchanged (the regression) and reads a three-frequency hop as one signal when the gaps are short
(a fixed 3,300 Hz target saw one pulse of it, 2026-10-15-hop-check). With 60 ms gaps the event merges but the amp class is weak:
the 100 ms inspection window around the peak holds one pulse of three, and the 2,800 and 3,800 Hz pulses are weaker (strength
~4.2-4.5k vs ~6k at 3,300 Hz, same session family). False answers with the band not yet measured.
