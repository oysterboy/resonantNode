# 2026-10-15 DET-013: which scalar parameters accept a 3-frequency hop (band 2800-3800), quiet room, 90 cm, up, 0.5 FS

E2 Analyzer COM5 (512-bin history + band bank, banner b309e96), E3 Emitter with hop. Hop 2800,3300,3800 Hz (3 pulses), `SEQ
band=2800-3800`, 10 trials per cell. Parameter sets: A defaults; B debounce 40, max_gap 60, max_duration 400; C debounce 80,
max_gap 100, max_duration 500, coverage 40; D debounce 130, max_gap 130, max_duration 600, coverage 40.

15 ms pulses:

| cell | result | notes |
|---|---|---|
| A, gap 20 | 0/10 duration_too_short | first pulse only |
| B, gap 20 | 5/10 (5 coverage_too_low) | |
| C, gap 20 | 10/10, amp medium | |
| C, gap 60 | 0/10 pattern_rejected, amp weak | merged, inspector rates it weak |
| D, gap 60 | 0/10 pattern_rejected, amp weak | |

Denser hops (longer pulses):

| cell | result | duration median |
|---|---|---|
| A, 30 ms x3, gap 10 | **10/10**, amp medium | 96 ms |
| A, 30 ms x3, gap 20 | **10/10**, amp medium | 117 ms |
| A, 40 ms x3, gap 20 | 8/10 (2 duration_too_long, default max 200 ms), amp medium | 196 ms |
| C, 30 ms x3, gap 40 | **10/10**, amp medium | 206 ms |

Sparse hops (15 ms pulses with long gaps) are limited by the amp inspector's 100 ms window; a stepped hop of ~100-120 ms is accepted
with the default scalar parameters, so the false-answer cost of the permissive set (2026-10-15-band-noise: 4 speech false answers)
is avoided.
