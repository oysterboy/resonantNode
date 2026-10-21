# 2026-10-21: five nodes free-running on the shipped build faaa814 (0.5 FS), RB CALL kicks (5g gate, 5f measurement)

Setup:
- All five boards on main faaa814, the first build with the 0.5 FS default
  (decisions/2026-10-21-damp-tone-level-0p5.md). Idle voice on; shipped behavior values; `# state` lines in the
  header.
- Layout as for the trial (bench/sessions/2026-10-21-hearing-matrix).
- `tools/bench/nodegroup.py --kick E1`: RB CALL at 15 s, then every 90 s; 360 s; RB STATUS every 120 s.
- The owner had left the bench.

## 5g gate

- RB CALL: 4 calls, each played through Behavior (`source=call`, the hop train). Each was answered by another
  node within 3 s.
- Idle call: 27 idle calls (`source=idle`, the 300 Hz / 500 ms announcement, then the node's hop train; the
  `output.idle_announce_*` params are in the header). **All 27 were answered.**
- Rotation table: verified at boot earlier the same day (selftest-all5).
- **The 5g gate is met.**

## 5f: the third question as counts

| count | value |
|---|---|
| emissions | 420 in 360 s; per minute 72, 84, 51, 66, 59, 88 |
| per node | E1 77 answers + 7 idle, E2 77 + 8, E3 72 + 7, E4 78 + 3, E5 85 + 2 |
| rests | 79 in total (rest_field_dense 42, rest_own_budget 30) |
| chains after a start | end within 2.9-7.7 s, with 14-29 emissions each |
| longest silences | 19.2, 18.3, 17.8, 17.1, 17.0 s |
| time to quiet (15 s gap within 30 s) | 13 of 31 starts, median 4.1 s: **FAIL** against the 80 % pass mark |

How to read the FAIL:
- Every chain ends, because the rest rule ends it within about 4 s. None runs away.
- The group still rarely gets 15 s of silence. Five nodes with idle timeout 20 s ± 10 s give the group an idle
  call about every 13 s (27 in 360 s), and at 0.5 FS each one wakes everybody.
- Part of the 31 "starts" are duplicates: two or three nodes idle within the same second, and the tool lists
  them as separate starts with 0-2 emissions. That inflates the failures. The 13 quiet starts are real.
- The pass mark (2026-10-17) was written on the four-node bench at 0.3 FS, where idle calls reached fewer
  listeners.
- This is a measurement, not a tuning pass. The levers are the owner's call: a longer idle timeout, or the pass
  mark measuring "the chain ends" instead of "15 s of silence".

M (the per-minute ceiling) is still open. This session's maximum was 88 emissions per minute, and the owner sets
M.

Tool notes:
- The listener <- sender table undercounts: an answer's `emit start` line and the listener's verdict often reach
  the PC within the same 10 ms, so "unattributed 41" is mostly attribution, not false triggers. There was no
  noise source in the room.
- After the run all five boards were held silent by a background process on the lab PC (ports open, idle off,
  onset 200000), so they do not beep overnight. Closing that process resets them, and their idle voice comes back.
