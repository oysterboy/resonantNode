# 2026-11-16 evening: glide against cricket on five windows, detection rate

`tools/bench/callmatrix.py` (main 3ab2dd4b) through E0 (COM12): per cell the call's preset (`glide`, `cricket`),
one window (band_per_node 1, the range one window wide, nothing moves), R1 and E2 playing, E3 muted listening,
10 min, live only. Images 6e56f53b. Each window ran glide then cricket back to back, 19:25-21:10. E3 is noisy
(`e3_selftest.md`); its column says more about E3 than about the call. Per-cell logs `cell_<call>_<window>.log`,
counts in `summary.csv`, times in `changes.txt`.

Rate: with two players, A -> B is B's valid detections over A's calls (window-free); E3: its valid over all calls.

| window (Hz) | glide R1->E2 | glide E2->R1 | cricket R1->E2 | cricket E2->R1 | glide E3 | cricket E3 |
|---|---|---|---|---|---|---|
| 250-1250 | 87 % | 82 % | 56 % | **13 %** | 44 % | 27 % |
| 1250-2250 | 86 % | 85 % | 87 % | 79 % | 44 % | 82 % |
| 2500-3500 | 87 % | 86 % | 82 % | 76 % | 88 % | 53 % |
| 4000-5000 | 89 % | 84 % | 89 % | 93 % | 89 % | 92 % |
| 5500-6500 | 79 % | 77 % | 38 % | **0 %** | 60 % | 50 % |

Median length of the valid detections (ms):

| window | glide R1 / E2 / E3 | cricket R1 / E2 / E3 |
|---|---|---|
| 250-1250 | 115 / 99 / 87 | 417 / **121** / **86** |
| 1250-2250 | 84 / 83 / 84 | 383 / 383 / 383 |
| 2500-3500 | 86 / 88 / 94 | 385 / 387 / 389 |
| 4000-5000 | 120 / 101 / 102 | 420 / 426 / 405 |
| 5500-6500 | 158 / 132 / 139 | none / 432 / 446 |

## What it shows

- The glide holds 77-89 % on every window. The cricket matches it on 1250-2250, 2500-3500 and 4000-5000 (76-93 %)
  and fails on the two outer windows.
- **High (5500-6500): the call reads too long.** Every board hears a longer sound up there (the glide 132-158 ms
  for 75 ms played, against ~85 ms in the middle): the room rings longer at high pitch. The 375 ms cricket then
  reads 432 ms at E2 and 446 at E3, against the 450 ms maximum; R1, which reads the glide longest (158), logged no
  detection at all in 10 min: its reading very likely ran past 450, which the detector drops without a verdict
  (not printed, so inferred).
- **Low (250-1250): the trill is not heard, or not accepted.** E2 and E3 read 121 and 86 ms: only the glide, the
  trill on ~250 Hz below what the speaker / mic carry there (the receive roll-off below ~1250 Hz,
  docs/research/room-comb-and-margin.md). R1 hears the whole 417 ms call but rejected 27 of 33
  (`ignored_invalid_pattern`; the Node does not print which check failed). A likely reading, not checked: the low
  trill rings on in the room and stays the strongest pitch through the climb, so the pattern check sees one pitch.
- E3 swings between cells (44-92 %) with no pattern by call; its noise (e3_selftest.md) dominates.
- Calls per cell fall with the cricket where it is not heard (45 against 97 at 250-1250): a node that does not hear
  the other answers less, so the chains are shorter.

## Open

- The cricket's length against the 450 ms maximum: a shorter trill (fewer pulses or shorter gaps) for the high
  windows, or a longer maximum, which is a detection setting (the inspection look-back, 576 ms, allows at most
  576 - 20 debounce - 100 window = 456 ms without a larger history build). Not changed here.
- R1's rejections at 250-1250: needs the reject reason printed, or an Analyzer capture.
