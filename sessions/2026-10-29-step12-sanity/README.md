# 2026-10-29: step 12.1, the sanity check bench test on ba34561

The standing check (owner 2026-10-29: the scripted set), all remote, all five boards on ba34561 (delivered by OTA).

| part | tool | result |
|---|---|---|
| Node: boot, every command, RB STATUS keys vs 40c92e3, outputs-off, 10 hop trains answered, PARAM save / reboot / load, radio PING and PARAM SET, SELFTEST incl. external and by radio | tools/bench/nodecheck.py | nodecheck/: 24 / 27 (the quiet gate, and the tool's fixed SELFTEST waits); nodecheck2/ after the tool fix: 27 / 27 |
| Analyzer SEQ set incl. the room check (hop10, chirp 3200 / 100, chirp 3450 / 150) and the edge cases | tools/bench/analyzerab.py | ab_ba34561/: against 2026-10-28-step11g-ab/after_184c72c (seqcmp): the same trial results, hop 10 / 10, chirps 10 / 10 rejected, no key differences; start during a run refused, start + STOP does not start |
| OTA round trip with token | otaserve.py, NET * OTA + USB | all five confirmed (app0 / app1) |

12.1.1, the quiet gate (`RB rebase` waits for smooth < 400 for 1 s within 8 s): nodecheck's first run missed it on E1's
boot (smooth peaks 391-486) and E4's / E5's SELFTEST reported quiet_boot FAIL (their last boot: the simultaneous OTA
reboot of all five, then E5's rollback tests). Median smooth 108, p90 ~300, so the margin to 400 is thin and a short
noise burst within the second resets the hold. Measured afterwards in a quiet moment: 15 / 15 quiet rebases ok (5 per
Node, smooth max 182-249). A Node that misses the gate still runs on the warmup rebase (state FAILED_NO_QUIET,
outputs allowed); only SELFTEST's quiet_boot reports FAIL. Decision pending (owner).

12.1.1 after the owner's (b), 053e00b (the quiet gate gives up after 20 s, not 8): `NET * OTA` rebooted E1, E4, E5
together onto 053e00b; all three passed the quiet-gated rebase and went active (`RB rebase done quietMs=1000-1004`).
The offset moved a lot between the 2 s warmup rebase and the quiet one: E1 -474 -> -1,614, E4 1,270 -> 611, E5
1,384 -> -44 (PCM counts): the second rebase is what catches the settled mic.
