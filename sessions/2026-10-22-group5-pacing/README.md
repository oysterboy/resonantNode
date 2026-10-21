# 2026-10-22: five nodes on 908dc9a, the new network pacing (rest 5 s, idle call 60 s +- 20 s), 5f pass mark

Setup:
- All five boards on main 908dc9a: 0.5 FS; restMs 5,000; idle timeout 60 s +- 20 s
  (decisions/2026-10-22-network-pacing.md). `# state` lines are in the header.
- Trial layout (bench/sessions/2026-10-21-hearing-matrix).
- `tools/bench/nodegroup.py --kick E1`: RB CALL at 15 s, then every 90 s; 600 s; RB STATUS every 120 s.
- The owner was away.

| count | value |
|---|---|
| emissions | 274 in 600 s (8 idle calls, 7 RB CALLs, the rest answers); per minute 16, 48, 24, 6, 42, 23, 27, 37, 23, 22 |
| rests | 42 (rest_field_dense 34, rest_own_budget 8) |
| chains | 13 distinct starts; length median 4.0 s, max 4.6 s |
| **pass mark** (the chain ends within 30 s in 80 %) | **13 of 13: PASS** |
| **M = 100** | max 48 per minute: **PASS** |
| 15 s silence within 30 s (counted only) | 13 of 15 starts |

Against 2026-10-21 (same layout and level, old pacing): the old pacing gave 51-88 emissions per minute and an
idle call about every 13 s. The new pacing gives 6-48 per minute and an idle call about every 75 s; the 15 s
silence now comes after 13 of 15 starts (13 of 31 before). Every chain still ends in about 4 s.

After the run, all five boards were held silent again by the background process on the lab PC.
