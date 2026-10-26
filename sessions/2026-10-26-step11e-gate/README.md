# 2026-10-26: step 11e bench gate (review fixes, PR #64 merged as 597c921)

Layout: E2 Analyzer / E3 Emitter at one height, 79 cm (owner 2026-10-23); E1/E4/E5 Nodes silent (stored); nobody
at the bench. Before = bc409a5 (main before the merge, built in a separate worktree), after = 597c921.

- Analyzer A/B, SEQ 10 trials mode=detail: pre_hop10 / post_hop10 10/10 expected; seqcmp_hop10.txt identical in
  every count and line type; every SEQ_* key set and the diagnostics values (report_matched 1, trial attribution
  1..10, integrity none, SEQ_HISTDBG 0) identical.
- chirp (dur=100): rejected 10/10 duration_too_short (reads 31-46 ms) before, after, and with the old Emitter too
  (pre_chirp10_pre_emitter): the room at this geometry, not the pass; seqcmp_chirp10.txt.
- PARAM SET on the Analyzer (after): onset 20000 -> post_hop10_onset20000 10/10 miss; 2000 -> 10/10 expected;
  `abc` -> parse_error, value kept (post_param_bad).
- Node smoke on E1 (nodesmoke.py / nodesmoke.out): boot, RB STATUS 0 drops, 10/10 Emitter trains heard and
  answered, PARAM RESET with silent saved -> the node sounds again; left silent (stored).
- radio round trip: netcheck/netcheck.txt (E1 gateway, E4 / E5), every step ok, 0 drops except E4's SAVE (9).
- on-device suites on E2: test_analyzer_pass_rules 9/9, test_timebase 8/8.
Left: all five boards as Nodes on e5d15e1 code, output.silent=1 stored.
