# 2026-10-17 rest rule on the bench: the E1-E4 ping-pong, before and after

E1 (COM6) and E4 (COM7) flashed to main 2ddf2b4 (rest rule: >= 4 valid patterns in the field window or 6 own
emissions in 10 s start a hard 10 s rest), E2/E3/E5 held silent (`tools/bench/pairmatrix.py`), placement as in
2026-10-17-pairmatrix (E1-E4 40 cm, same height), idle voice on, 180 s, no kick.

| Run | E1 emissions | E4 emissions | heard E1->E4 / E4->E1 | rests |
|---|---|---|---|---|
| before (2026-10-17-pairmatrix, previous build) | 259 | 259 | 259/259, 258/259 | - (one chain of 518 turns) |
| after (this session) | 29 | 30 | 28/29, 29/30 | 8 (7 field_dense, 1 own_budget) |

Emissions per 20 s stay at 1-5 per node: short exchanges of a few turns, then 10 s of rest, then an idle call
starts the next one. The rest starts with `field_patterns=4` as designed; once the own budget fired first (E4 at
82 s, field 3). Hearing is unchanged (28/29 and 29/30): the rule removes the endless chain, not the contact.
