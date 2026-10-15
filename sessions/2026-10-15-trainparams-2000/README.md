# 2026-10-15 DET-011/DET-012: trains at a 2,000 Hz target, quiet room (512-bin history, 90 cm, up, Emitter 0.5 FS)

E2 Analyzer COM5 (512-bin FeatureHistory, 22fdba4), E3 Emitter PULSES 0.5 FS. Train params = max_gap 130, max_duration 600,
min_release_coverage 40, release_debounce 130. Music off; noise runs at 2,000 Hz with these params were not taken.

| Run | Result | detector sees (median dur / coverage / island / max gap, ms) |
|---|---|---|
| plain chirp, default params | 10/10 expected | 120 / 120 / 120 / 0 |
| 15 ms x3, gap 20, train params | 10/10 expected | 95 / 73 / 25 / 9 |
| 15 ms x3, gap 60, train params | 0/10 pattern_rejected | 207 / 72 / 24 / 31 |
| silence, train params (50 windows) | 0 accepts | - |

At 2,000 Hz the gap-20 train is accepted; the gap-60 train merges (207 ms event) but the evaluator rejects it (the strength
at 2,000 Hz is lower than at 3,300 Hz, where the same train passed). Whether a train lowers the speech / music false answers
is not tested: the Scalar definition has no train criterion (event count / island count), so the permissive params can only
add false answers.
