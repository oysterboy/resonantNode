# 2026-10-17 rest-rule tuning, four Nodes, 300 s per variant (tools/bench/nodegroup.py --setup)

Same boards and placement as 2026-10-17-group4-rest-rule (E1-E4 on 2ddf2b4, E5 held silent, idle voice on),
settings sent live with `RB BEHAV` after the boot (confirmed on all four boards in each log); code defaults
unchanged. The field window (FieldStateConfig::patternWindowMs, ~4 s) is a build value, not tuned here.

| Variant | rest | field threshold | group emissions/min | answers / idle calls (4 nodes) | longest silences |
|---|---|---|---|---|---|
| defaults (group4_600s) | 10 s | 4 | 29-43 | 299 / 48 in 600 s | 20.6, 19.5, 19.2 s |
| A_rest30s | 30 s | 4 | 15-38 | 118 / 28 | 29.8, 20.1, 19.2 s |
| B_field2 | 10 s | 2 | 16-22 | 68 / 29 | 19.5, 17.4, 17.3 s |
| C_rest30s_field2 | 30 s | 2 | 13-19 | 51 / 32 | 20.8, 19.7, 19.3 s |

Reading: the lower field threshold matters more than the longer rest (B halves the traffic, A barely changes it);
together (C) the group runs at 13-19 emissions per minute, about half of the defaults. But no variant produced a
silence of 30 s or more: in C the idle voice is 39 % of all emissions (four nodes calling every 20-25 s), and every
idle call can start a short exchange. A quiet network therefore needs the idle voice sparser too (idleTimeout), or a
rest that also covers idle for the nodes that did not rest.
