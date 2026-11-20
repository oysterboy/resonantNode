# 2026-11-19 bands: bb-all, cricket, fieldcricket across five windows

R1 (stripboard lr2, USB) and E2 (D-AMP, radio) at 1 m, gateway E0 on COM12; one-way hearing test, each node on its
own bins (`--listen own`), 5-min blocks, 2 rounds, 20:20 to 01:35. About 235 calls per cell.

| Window | R1->E2 bb-all | cricket | fieldcricket | E2->R1 bb-all | cricket | fieldcricket |
|---|---|---|---|---|---|---|
| 250-1250 | 97 % | 100 % | 100 % | 95 % | 97 % | 98 % |
| 1250-2250 | **11 %** | 100 % | 100 % | 99 % | 99 % | 99 % |
| 2500-3500 | 100 % | 100 % | 100 % | 99 % | 98 % | 99 % |
| 4000-5000 | 100 % | 100 % | 100 % | 98 % | 98 % | 99 % |
| 5500-6500 | 100 % | 100 % | 100 % | 97 % | **56 %** | 93 % |

Two holes, each one call in one direction, both rounds alike (not a drift of the room):

- bb-all R1->E2 at 1250-2250: 202 of 227 invalid, `support_too_low/amp` (the amplitude check: too few of the call's
  bins loud enough). E2 hears R1's tonal calls in the same window at 100 %, so it is the broadband part from R1
  there, not the path.
- cricket E2->R1 at 5500-6500: 104 dismissed `duration_too_long`. The call (375 ms) plus what the room adds at that
  pitch passes the 450 ms maximum; fieldcricket (the faded pulses) loses less (93 %), bb-all none.
