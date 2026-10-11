# 2026-10-11 soak at 70 cm (#27, second attempt): stopped by the micros wrap

Setup: Emitter E3 (COM4, emitter build with SELFTEST, git 8fd6b6f+uncommitted
= be61884 source) -> Analyzer E2 (COM3, 652c9a6), 70 cm, speaker faces mic,
0.3 FS, 3200 Hz. SELFTEST gate before start passed on both
(2026-10-11-selftest-soak-gate2). Runner soak.py at f460680 (the session's
third note says "(31d...)": a placeholder, read f460680). Started 11:14,
stopped by hand at 13:3x after 10 blocks (owner decision).

| Blocks | Time | T3 expected / 10 | median strength | error |
|---|---|---|---|---|
| 1-4 | 11:14-11:59 | 10 each | 9,853 / 10,536 / 8,920 / 9,322 | - |
| 5 | 12:14 | 8 (2 rejected) | 12,480 | - |
| 6-10 | 12:29-13:29 | 0 each | - | no_detection; Analyzer SELFTEST mic PASS, amp PASS |

0 dropped buffers and 0 OBS detections in every block.

Block 6 started at Analyzer uptime 4,509 s, block 5 ended at 3,638 s:
micros() wrapped at 4,295 s in between, exactly as in the 2026-10-10 soak.
The automatic SELFTEST (new in f460680) proved mic and amp fine, which
turned the earlier "loose contact" reading into a firmware finding:
NODE-017 (first filed as NODE-016; renumbered).
