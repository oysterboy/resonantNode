# 2026-11-18 hearing test, R1 and E2 at about 1 m

One way at a time (tools/bench/hearingtest.py, main 3c2c2c3d for the scoring): the listener muted (it still
detects), the player calling on a 2.5 s silence timer (spread 1 s), so each call is one independent trial. Four
calls, both directions, 5 min a block, two rounds in reversed order, 16 blocks, 22:46-00:12. Window 2500-3500 Hz on
both. Images: main f5106956 (R1 by USB, esp32dev-lr2; E2 by radio OTA, esp32dev). Quiet room: input peak at rest
R1 ~130-190, E2 ~30-40, onset 2000.

| Call | R1 -> E2 | E2 -> R1 |
|---|---|---|
| bb-all | 235 / 235, 98-100 % | 234 / 235, 98-100 % (1 detected) |
| bb-trill | 231 / 231, 98-100 % | 236 / 237, 98-100 % (1 detected) |
| cricket | 232 / 232, 98-100 % | 232 / 233, 98-100 % (1 detected) |
| glide | 240 / 240, 98-100 % | 228 / 238, 92-98 % (10 detected) |

No invalid verdict, no detector reject, no call missed. "Detected": R1's inspect line arrived (every check passing)
but its verdict line was lost on the radio, so the rate leaves it out; the node did hear.

Notes:
- The first scoring (ht.out lines in changes.txt) counted the 300 Hz announce tone of each idle call as a call of
  its own, so every rate read ~50 %. Re-scored with `--rescore`; summary.csv and calls.csv are the corrected ones.
- E2 logs `RB reject reason=duration_too_short` (~12 ms, strength ~2300) on R1's announce tone, every call: the
  stray column of R1 -> E2. R1 does not do this on E2's announce.
- Against the both-playing run of the same evening (2026-11-18-bb-all-1m, 77-95 %): that dip came from its counting
  (answer chains, overlapping calls), not from the calls.
