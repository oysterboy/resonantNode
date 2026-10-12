# 2026-10-12 soak at 70 cm (#27, third attempt): 7.3 h, accepted short

Setup: Emitter E3 (COM4, emitter build 8fd6b6f+be61884 source) -> Analyzer
E2 (COM3, 189b24c with the TimeBase fix, #30), 70 cm, speaker faces mic,
0.3 FS, 3200 Hz. SELFTEST gate before start passed on both
(2026-10-12-selftest-soak3-gate: E2 54.7 dB, E3 60.3 dB). Runner soak.py at
f460680. Started 08:00:37 with one Analyzer reset; stopped by hand at 15:20
after block 30 of 96 (owner: accept the soak with the runtime reached). No
room conditions were recorded.

## Result

30 blocks, 0 missing, 0 errors, 0 no_detection, 0 dropped DMA buffers.
T3: 299/300 expected, 1 rejected (block 29), 0 miss / late / early;
avg dt 19-28 ms per block. OBS: 0 detector accepts in 150 silent windows.

Real-uptime gate of #30: the 32-bit micros wrap (Analyzer uptime 4,295 s)
fell at ~09:12, between blocks 5 and 6. Every block after it detected
10/10 (blocks 6-30, Analyzer uptime up to ~26,000 s, six wraps' worth).
Both earlier attempts went 0/10 from the first block after the wrap.

| Hour | blocks | median strength, mean | range |
|---|---|---|---|
| 08 | 4 | 9,104 | 9,064-9,192 |
| 09 | 4 | 9,626 | 9,122-10,018 |
| 10 | 4 | 10,111 | 9,934-10,487 |
| 11 | 4 | 11,127 | 10,414-11,388 |
| 12 | 4 | 11,218 | 11,036-11,727 |
| 13 | 4 | 11,197 | 10,692-11,742 |
| 14 | 4 | 10,024 | 9,278-10,900 |
| 15 | 2 | 10,881 | 10,845-10,916 |

Over all blocks: mean 10,380, sd 845 (CV 8.1 %), min 9,064, max 11,742.
Block-to-block step: median 192, max 1,945 (13:45 -> 14:00).

## Reading

Accept rate does not vary: 10/10 in 29 blocks, 9/10 in one. Strength does
vary by more than block-to-block noise: a slow rise of ~23 % from 08:00 to
midday (9.1k -> 11.2k, hour means), a dip at 14:00-14:15 (9.3-9.8k) and a
partial recovery. Neighbouring blocks usually differ by ~200, so the hourly
trend is about ten times the block-to-block step. With one partial day and
no room notes, it can't be attributed to temperature, daylight or anything
else; it does bound the problem: a same-setup comparison made hours apart
can differ by ~25 % in strength with no change in detection at 70 cm.

Not covered: the night half of a 24 h day, a second day, TonalPulseFreq.
The owner reported the board "occasionally beeps" during the run; not
diagnosed (expected: one ~1 min burst per block at :00/:15/:30/:45 +37 s;
nothing reads E3's port, so an Emitter reset into AUTO would not show here).
