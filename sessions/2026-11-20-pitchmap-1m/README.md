# 2026-11-20 pitch map at 1 m: trill alone, every pitch 2500-3700 Hz in 50 Hz steps

R1 (stripboard lr2) and E2 (D-AMP) at 1 m, gateway E0 on COM12, 03:00-05:09. Preset `trill` (11 x 20 ms pulses),
every node on the whole window (`--listen band`), the player pinned per block (`--pitches`), five windows shifted by
50 Hz, 60-s blocks, 2 rounds: about 46 calls per pitch and direction.

| Pitch | R1->E2 | E2->R1 | | Pitch | R1->E2 | E2->R1 |
|---|---|---|---|---|---|---|
| 2500 | 48/48 | 45/47 | | 3100 | 45/45 | **0/47** |
| 2550 | **0/47** | 44/46 | | 3150 | **0/48** | **0/47** |
| 2600 | **0/46** | **0/45** | | 3200 | 47/47 | 45/46 |
| 2650 | 45/45 | 45/46 | | 3250 | 45/45 | 43/46 |
| 2700 | 46/46 | 46/47 | | 3300 | 49/49 | **0/49** |
| 2750 | 48/48 | 48/48 | | 3350 | **0/46** | 47/47 |
| 2800 | 46/46 | 44/48 | | 3400 | 44/44 | 31/44 |
| 2850 | 47/47 | **0/45** | | 3450 | 46/46 | 44/47 |
| 2900 | 45/45 | **0/45** | | 3500 | 47/47 | 47/47 |
| 2950 | 42/42 | **0/44** | | 3550 | 45/45 | **0/46** |
| 3000 | 44/44 | 8/47 | | 3600 | 46/46 | **0/47** |
| 3050 | 44/45 | 38/46 | | 3650 | 42/42 | 49/49 |
| | | | | 3700 | 45/45 | 45/46 |

R1->E2 loses 4 of 25 pitches, E2->R1 10 (and 3 partly); 11 pitches work both ways. A pitch fails all or nothing, the
same in both rounds, and the two directions differ: the node's speaker or mic at that pitch, not a room null between
them. The detector's measurements say why: the failing calls are dismissed `longest_island_too_short` with
island_ms 16-19 (2550 and 2600 R1->E2): the 20 ms pulse arrives a few ms short of the 20 ms longest-stretch rule
where the node is weaker. Gaps measured 3-17 ms, inside the 20 ms tolerance. 3100 E2->R1 is `strength_too_low`.
Follow-up: bench/sessions/2026-11-20-trillfix-1m (longer pulses, the rule lowered).
