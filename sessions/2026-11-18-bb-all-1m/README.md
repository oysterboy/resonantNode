# 2026-11-18: the broadband click against the plain trill and glide, R1 and E2 at about 1 m

Setup: R1 (stripboard v2, image 54de01a4, mic fixed by the owner the same afternoon, SELFTEST pass 43 dB) and E2
(D-AMP, image cc9832f7 by radio OTA) about 1 m apart; one window for both, 2500-3500 Hz (behavior.band_per_node 1),
nothing moving. Each call 10 min, applied live with tools/bench/callmatrix.py; R1 and E2 both play. E3 was meant as
a listener but stopped answering by radio when the first cell started (the owner was looking at its BOOT-pin
capacitor), so the comparison ran without it.

| call | R1 calls | E2 calls | R1 -> E2 | E2 -> R1 | invalid |
|---|---|---|---|---|---|
| bb-all (click on every pulse) | 66 | 71 | 83 % (55) | 77 % (55) | 0 |
| bb-trill (click on the trill only) | 76 | 76 | 86 % (65) | 80 % (61) | 0 |
| cricket (plain trill) | 90 | 111 | 100 % (90) | 86 % (95) | 0 |
| glide (reference) | 148 | 155 | 95 % (141) | 92 % (143) | 0 |

Rate: the listener's valid detections over the other player's calls (window-free, as in 2026-11-16-call-matrix).

- No call produced an invalid verdict at either board.
- The broadband click costs detection here: both bb variants sit 6-17 points under the plain trill, bb-all lowest.
  With 60-110 calls a cell, a few points are noise; the order bb-all < bb-trill < cricket holds in both directions.
- Glide is the steadiest (92-95 %) and calls about twice as often in the same 10 min: a shorter call (75 ms
  against 375 ms) means shorter turns, so more of them.
- Against 2026-11-16 on the same window (cricket R1/E2 83-84 %, other distance): cricket is higher at 1 m.

Logs: cell_bb-all_2500-3500.log, compare/cell_<call>_2500-3500.log; summary.csv in both folders.
