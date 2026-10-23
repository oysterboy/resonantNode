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

First 2 min: 24 emissions, every node started emissions and heard others, 0 dropped buffers.
Results: when the run ends.
