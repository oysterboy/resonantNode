# 2026-10-15 DET-011: pulse trains with raised gap / duration / debounce params

Same setup as 2026-10-15-pulses-timing. `PARAM` sent before each `SEQ start` in the same run (the params apply:
10 ms/gap 20 moved from coverage_too_low to longest_island_too_short).

W set: scalar_max_gap_ms=130, scalar_max_duration_ms=600, scalar_min_release_coverage_ms=40.
D set: W plus scalar_release_debounce_ms=130.

| cell | param set | result | detector sees (median dur / cov / island / max gap) |
|---|---|---|---|
| 15 ms, gap 60 | W | 0/10 duration_too_short | 37 / 32 / 29 / 4 |
| 15 ms, gap 115 | W | 0/10 duration_too_short | 37 / 32 / 28 / 4 |
| 10 ms, gap 20 | W | 0/10 island_too_short x5, pattern_rejected x5 | 88 / 74 / 16 / 4 |
| 10 ms, gap 60 | W | 0/10 duration_too_short | 28 / 24 / 15 / 4 |
| 15 ms, gap 60 | D | 0/10 **pattern_rejected** (evaluator) | 217 / 123 / 30 / 22 |
| 15 ms, gap 115 | D | 0/10 **pattern_rejected** (evaluator) | 325 / 127 / 29 / 63 |
| 10 ms, gap 60 | D | 0/10 longest_island_too_short | 174 / 74 / 14 / 36 |

The detector only keeps an event open across a gap if the release debounce is longer than the gap; max_gap
alone does nothing for the closing decision. With the debounce at 130 ms the whole 3-pulse train becomes
one event (217-325 ms) but the evaluator then rejects it with contrast and amp classes unknown (the
inspectors give no class), so the trains are not yet usable. Not run: false answers (speech/music).
