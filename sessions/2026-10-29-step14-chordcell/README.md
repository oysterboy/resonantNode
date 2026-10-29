# 2026-10-29: step 14, chord bench cell (two Nodes answer at once, one listens)

E3 Emitter plays hop10 trains (6 s apart, 15 per cell); E4 (rotation 3) and E5 (rotation 4) answer; E1 silent,
listening; layout of 2026-10-26 (E5 -20 cm- E4 -30 cm- E1; E2 / E3 at 79 cm). Nodes on 5f1aa75, Emitter 0e8b3bd.
tools/bench/chordcheck.py: idle calls off and the answerers unsilenced live (not saved), E5's
behavior.wait_after_heard_ms raised by the cell's offset; PARAM LOAD at the end.

| cell | E1 valid verdicts per train | answers E4 / E5 |
|---|---|---|
| E4 alone | 2 (15 / 15) | 15 / 0 |
| E5 alone | 2 (15 / 15) | 0 / 15 |
| both, same wait (100 ms) | 2 (15 / 15): the chord counts once | 15 / 15 |
| both, E5 +50 ms | 2 (15 / 15) | 15 / 15 |
| both, E5 +150 ms | 1 (15 / 15): the chord is lost | 15 / 15 |
| both, E5 +250 ms | 3 (15 / 15): two answers | 15 / 15 |

Matches the host mix (2026-10-29-step14-rotations, chordmix_offsets.csv): one occurrence up to ~100 ms apart,
nothing at 120-220 ms (merged past max_duration_ms 300), two from ~230 ms.
