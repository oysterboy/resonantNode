# 2026-10-29: step 14.1, board checks after the review fixes (b006c00 on all five boards)

Layout of 2026-10-26 (E5 -20 cm- E4 -30 cm- E1; E2 / E3 at 79 cm). Before these runs the Nodes still had
detection.scalar_max_duration_ms = 300 stored from an earlier PARAM SAVE (boot: "PARAM stale ... using=stored"), so the
first chord cell on 915f564 (2026-10-29-step14-maxdur450/chordcell) ran at 300. 450 was then set and PARAM SAVEd on E1,
E4, E5 (E1 and E5 also stored 8 params that had no stored value yet, at today's defaults).

| part | result |
|---|---|
| chordcell/ (tools/bench/chordcheck.py, 15 trains per cell) | E1 valid verdicts per train: alone 2, same wait 2, **+150 ms 2, +200 ms 2** (at 300 both: 1, the chord lost), +250 ms 3 |
| nodecheck/ (tools/bench/nodecheck.py) | 26 / 26 PASS |

Noise at 450 (2026-10-29-step14-maxdur450/noise_all450, E1 at its stored 300, E4 / E5 at 450 live): 0 verdicts on all
three over two plays; hop trains 10 / 10.
