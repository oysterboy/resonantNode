# 2026-10-23/24: step 10c block 2, five Nodes free-running like an installation (24 h logged)

Owner 2026-10-23: "run all as nodes, installation like"; counted as 10c block 2 (owner: yes), moved from Sunday.

Setup:
- All five boards on Node dbb6305 (10d persistence, 10e fixes, 10f SoundOutput, drift and level fields), stores
  cleared by PARAM RESET: shipped defaults, roster rotations E1..E5 = 0..4, none silent.
- `nodegroup.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 --secs 86400 --status-every 120
  --drift-bin 3600`, started 2026-10-23 ~16:51 Berlin, ends ~16:51 on 2026-10-24. No kick: idle calls only.
- Layout: the trial layout of step 6 / 10b / block 1.

Room notes (owner, add as things change):
- 2026-10-23 ~16:55: E2 and E3 at the same height again (E3 back down after the height cell); E2-E3 distance 79 cm.
- 2026-10-24 ~07-09: owner briefly in the room (E5 <- E2 saturates for a moment then).
- Owner: otherwise nothing changed in the room (asked about ~20:00 on 10-23 and the first hour).

First 2 min: 24 emissions, every node started emissions and heard others, 0 dropped buffers.
## Results (24.00 h; results.txt from tools/bench/nodegroup_interim.py)

| gate item (step 6's pass marks + 10c) | result |
|---|---|
| chains ended within 30 s | **1752 / 1752** (max 5.8 s) |
| answers started by noise (no other emission in the 2 s before) | **0** of 30,546 |
| dropped DMA buffers / read errors / verdict-queue overflows | **0 / 0 / 0**, 719 STATUS lines per node |
| resets | **0**: audio.uptime 23.97 h on every node |
| deaf node | none: 303-418 valid verdicts per hour per node |
| max emissions per minute (M = 100) | 49 |
| lowest free loop stack | 12,060 B on every node (3,448 before 10e) |

32,496 emissions (6,301-6,686 per node, 352-421 idle calls each), 43,767 valid verdicts.

Per-board strength drift (median per listener <- sender per hour, unambiguous verdicts only: one sender's train
started 0.05-0.8 s before; see the table in results.txt):
- Hours 0-17: slow and smooth, mostly within +-15 % (e.g. E4 <- E5 23,624 -> 26,640, E3 <- E1 6,451 -> 5,694,
  E1 <- E3 10,318 -> 10,254). Detection did not change with it.
- One path stepped alone: E2 <- E5 from ~7,000-9,000 to ~4,200 in hour 3-4 (~20:00) and stayed. E5 was heard
  unchanged by the others and E2 heard the others unchanged, so neither board changed; the owner changed nothing
  in the room. Read as an interference null moving on that path (the height cell, 2026-10-23-cell-height25-110cm,
  shows how sensitive the train is to where nulls fall).
- From hour 18 (2026-10-24 ~10:50) many paths changed together and stayed (E5 <- E1 +40 %, E2 <- E3 +35 %,
  E5 <- E3 -30 %, E4 <- E2 -30 %): a change in the room; cause not yet noted (owner asked).
- E5 <- E4 is saturated (32,767) most of the run: that pair cannot show drift.

summary_nodegroup.txt is nodegroup.py's own end summary; its drift table predates the attribution fix (1dff962)
and mixes senders, use results.txt.
