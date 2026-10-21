# 2026-10-21: 5d gate run, two-node exchange on the shipped build with the `# state` header

Setup:
- E1 (COM8) and E4 (COM10) on Node ecfc72b, shipped values, idle off.
- E2, E3 and E5 held silent (ports open, idle off, onset 200000).
- `tools/bench/nodegroup.py --kick E1`: RB CALL at 10, 70 and 130 s; 180 s in total.
- The log header carries `# state` lines (RB BEHAV and PARAM DUMP) from both nodes, 56 lines.
- Placement is as left after the 2026-10-21 OBS runs; the owner did not record it.

Result, three kicks:
- Each kick gives the same chain: E1 calls, then E4, E1, E4, E1, E4, E1, E4 answer, 7 answers in a row.
- After that E1 enters a rest (`rest_field_dense`, field_patterns=4), and the pair is quiet 3.6 s after the kick.
- 8 emissions per minute.
- **The 5d gate is met:** an exchange of three or more answers in a row on the shipped build, with the state in the
  log header.

Tool note: the summary prints "E1 <- E4 0/12" and "unattributed 12". E4's `emit start` line and E1's verdict reach
the PC in the same 10 ms (serial buffering), so the attribution window (0.05-0.8 s after the sender's line) misses
them. The timeline above, from the log, is the reading. The window needs a lower bound at or below 0 s, as long as
the listener is not the sender.
