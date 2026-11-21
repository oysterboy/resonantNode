# 2026-11-20 night: five pitch maps on the line of four, 1 / 2 / 3 m at once

The line R1 - E2 - E4 - E3, 1 m apart (setup: ../2026-11-20-line-selftest; smoke test: ../2026-11-20-line-smoke).
All on dd22ab0a, hearingtest.py at 9eccebba: each node plays in turn while the other three listen, --listen band,
--pitches, --block 45, one round. 2026-11-20 21:00 - 2026-11-21 05:03, no reboot, every command answered.
Distances: 1 m R1-E2, E2-E4, E4-E3; 2 m R1-E4, E2-E3; 3 m R1-E3.

R1 is the prototype (stripboard, its own mic and case); the E nodes are D-AMP boards. Read R1 as a listener apart:
it hears worst at every distance while its own calls carry well, so the E pairs are the comparison.

This file holds the comparison of all five maps; the other folders' READMEs point here.

| Map | Windows | Pitches |
| --- | --- | --- |
| trill-taper (this folder) | 2500..2700 step 50, 1000 Hz wide | 25 |
| trill-taper-glide-grain-risefall | same starts, 1750 wide | 25 |
| trill-taper-glide-grain | 2500, 2600, 2700, 1000 wide | 15 |
| trill-taper-glide-grain-rise | 2500, 2600, 2700, 1750 wide | 15 |
| trill-taper-glide-grain-riseclear | 2500, 2600, 2700, 1750 wide | 15 |

Heard, all pitches pooled (in brackets: pitches at 90 % or more):

| Dist | Direction | trill-taper | -risefall | -glide-grain | -rise | -riseclear |
| --- | --- | --- | --- | --- | --- | --- |
| 1 m | E2->E4 | 99 % (25/25) | 91 % (20/25) | 97 % (14/15) | 86 % (11/15) | 86 % (13/15) |
| 1 m | E4->E2 | 100 % (25/25) | 86 % (19/25) | 89 % (12/15) | 82 % (11/15) | 92 % (13/15) |
| 1 m | E3->E4 | 100 % (25/25) | 83 % (18/25) | 100 % (15/15) | 93 % (13/15) | 98 % (13/15) |
| 1 m | E4->E3 | 96 % (24/25) | 81 % (18/25) | 85 % (10/15) | 72 % (10/15) | 77 % (9/15) |
| 2 m | E2->E3 | 78 % (17/25) | 75 % (13/25) | 65 % (8/15) | 73 % (8/15) | 78 % (11/15) |
| 2 m | E3->E2 | 98 % (24/25) | 69 % (16/25) | 73 % (9/15) | 89 % (12/15) | 79 % (11/15) |
| 1 m | R1->E2 | 100 % | 87 % | 94 % | 92 % | 77 % |
| 1 m | E2->R1 | 84 % | 89 % | 78 % | 85 % | 83 % |
| 2 m | R1->E4 | 99 % | 75 % | 89 % | 62 % | 71 % |
| 2 m | E4->R1 | 65 % | 33 % | 14 % | 32 % | 45 % |
| 3 m | R1->E3 | 69 % | 46 % | 29 % | 42 % | 40 % |
| 3 m | E3->R1 | 24 % | 16 % | 0 % | 27 % | 27 % |

What it says:

- trill-taper alone is the best map at every E distance: 96-100 % at 1 m, 98 % E3->E2 at 2 m. Every longer call
  (glide, grain, an ending) loses 5-20 points at 1 m.
- Among the endings, -rise and -riseclear do not beat plain glide-grain by much; -risefall is a little behind at 1 m.
- The 2 m losses sit at single pitches, not spread: trill-taper E2->E3 fails at 2500-2600 (38 / 44 / 13 %,
  invalid contrast) and 3050-3150 (6 % each) and is 100 % almost everywhere else; E3->E2 the same night is 100 %
  at those pitches but one. The other maps have holes near 2950-3200 and 2500-2650 too, in one direction or both.
  Pitch- and direction-dependent holes on a fixed line look like the room (standing waves, reflections), not the
  detector; a guess, not tested here.
- 3 m is out of reach for R1 as a listener; R1->E3 reaches 69 % with trill-taper.

Loud noise (../2026-11-20-line-falsealarm-loud, bench_audio.wav through the studio speaker, nobody calling,
20 min each, window 2500-3500): trill-taper let 16 false calls through (R1 12, E4 3, E2 1, E3 0);
-risefall 1 (E4). The shape check the endings carry does its job in noise; trill-only has none
(decisions/2026-11-19-trill-only-drops-the-shape-check.md). R1 and E2 stand nearest the speaker (peaks ~7,500-8,100
against ~3,500-4,500 at E4 and E3).

So the trade, from this night: trill alone is heard best, an ending keeps noise out. Per-pitch rows: summary.csv in
each map folder.
