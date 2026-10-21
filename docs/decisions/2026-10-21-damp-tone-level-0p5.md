# D-AMP tone level 0.5 of full scale

Status: decided 2026-10-21 (owner), implemented in the same commit. Supersedes 2026-10-09-damp-tone-level-0p3.md.
Date: 2026-10-21.

## Decision

The D-AMP tone plays at 0.5 FS (`I2S_TONE_AMPLITUDE`, default in `src/app/RuntimeDefaults.h`); it was 0.3. The
change applies to every D-AMP build: Node, Emitter, and the Analyzer's SELFTEST tone. The 5b batteries already
ran the Emitter at 0.5 through the build flag.

## Why

Step 5e (issue #44) measured a hearing matrix on the five trial boards in the owner's layout:
- E2 and E3 stand 110 cm apart; E1, E4 and E5 stand left of E3, 40 cm apart, starting 40 cm from E2;
- E1, E4 and E5 stand 10 cm lower than E2 and E3.

Each node played 10 RB CALLs; the listeners rested instead of answering (`tools/bench/hearmatrix.py`,
bench/sessions/2026-10-21-hearing-matrix).

| level | cells at 10/10 | worst |
|---|---|---|
| 0.3 FS (shipped until now) | 7 of 20 | E2 hears 2 of 40 calls in total; E3 hears 11 of 40 |
| 0.5 FS | 19 of 20 | E2 <- E1 0/10 |

At 0.3 the network falls apart into a well-connected group (E1, E4, E5) and two nodes that barely hear anyone.
This matches 2026-10-17, when contact was lost beyond about 1 m or across a 20 cm step. At 0.5 every node hears
every other except one cell. The amp is linear at 0.5 (2026-10-14: +4.4 dB, 27/50 -> 50/50 at 110 cm). The 5b gate
and its noise OBS ran with signals at 0.5 FS from the Emitter, without false triggers at room level.

## Rules out

- A per-node level, for now. The one weak cell, E2 <- E1, is a placement question for the trial, not a reason for
  a level param.
- Going above 0.5 without a new measurement: the noise cost and the amp's headroom at higher levels are not
  measured.

## Revisit

- If the trial room needs more range than this layout.
- If the noise OBS at 0.5 FS between Nodes (not Emitter to Analyzer) shows false triggers from neighbours' tails.
- If SELFTEST amp limits need rescaling: tones are now about 1.67x higher, and SNR and spread are the pass criteria.
