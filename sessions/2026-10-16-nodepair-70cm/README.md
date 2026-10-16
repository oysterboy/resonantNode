# 2026-10-16 two Nodes listening to each other, 70 cm (tools/bench/nodepair.py)

Both boards on the Node build a5a9bbb (band 2950-3950, hop train 25 ms x 5 gap 10). A = E2 (COM5, node_id
0xE84C4BC3DC24, rotation 4, order 3950,2950,3200,3450,3700), B = E3 (COM3, node_id 0x38B749C3DC24, rotation 3, order
3700,3950,2950,3200,3450; flashed with BOOT held). Mic and speaker facing up, 70 cm, quiet room. `RB log full` on
both, kick = one `SELFTEST chirp` on A at t = 5 s. Runs of 120 s (60 s for the last).

| Run | A verdicts / heard-answers / idle | B verdicts / heard-answers / idle | longest answer chain | latency heard -> emit |
|---|---|---|---|---|
| pair_kickA_120s (defaults) | 2 / 1 / 6 | 6 / 6 / 2 | 2 | median 119 ms |
| pair_kickA_noidle_120s (`RB BEHAV idle=0`, had no effect) | 7 / 5 / 3 | 3 / 3 / 5 | 2 | 119 ms |
| pair_kickA_refr100_120s (`refractory=100 idleEnabled=0`, idle still on) | 7 / 7 / 2 | 2 / 2 / 5 | 2 | 117 ms |
| pair_kickA_debug_60s (`RB debug events`) | 4 / 4 / 1 | 2 / 2 / 2 | 2 | 117 ms |

What works: a Node hears the other Node's hop train and answers with its own, 117-127 ms from the verdict to the
emission; A (E2) heard every one of B's emissions in the last three runs (7/7, 4/4), hop trains and idle chirps alike.
No block reasons in any `BEH verdict` line (`block=none` throughout, also with the default refractory 400 ms).

What does not: the exchange never goes beyond two (kick -> answer -> answer-back -> silence). B (E3) hears A's idle
chirps (6/6 in the first run) but almost never A's hop-train answers: 2 of 9, 2 of 7 in the later runs, and the Node
log shows nothing at all around them (no verdict, no reject; the Node build does not print detector-stage rejects,
`RB debug events` neither). So the chain breaks on B's side. Candidates: E3's mic hearing E2's speaker (the direction
E2 -> E3 has never been measured; every battery was E3 -> E2), or B's detector rejecting A's train (a bin of A's
order in a null at B's mic). `RB summary` after the last run: B pending=1 accepted=1 (its detector formed one candidate in 60 s, the one it
answered), A pending=3 accepted=3. So B's detector does not even see A's hop trains as candidates: the stream's energy
from E2's speaker at E3's mic stays under the onset threshold, or the whole train is missing at that position. To
decide: E2 as Emitter, E3 as Analyzer, hop5 on A's order, 10 trials with SEQ DIAG (needs BOOT on E3).

Also seen: `RB BEHAV idle=0` / `idleEnabled=0` did not stop the idle voice (idle emits continued; the command echoed
its line without error), to check in the command parser. Idle chirps every 20-25 s dominate a two-node exchange once
the chain has died; the idle pattern (2000 Hz then 3200 Hz, 500 ms each) is heard by the other node as a valid
single pulse.
