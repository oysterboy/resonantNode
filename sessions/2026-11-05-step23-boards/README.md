# 2026-11-05 night: step 23's board gates, by radio from E0

Five boards on main 7d8c4dfa (23.0-23.3), all pulled and flashed by NET OTA from E0.

## 23.2 the train unit: two answers ~100 ms apart (chordcheck.py)

`RB hop_check` on every board after a gap change: `heard_train_ms=125` for the glide (75 ms train + the 50 ms
room-tail margin); the printed value lags one setting behind inside the same command (syncBaseBin runs every loop
pass), so read it at rest. The level counts a merged chord as round(duration / unit), unit 125.

chord.txt (E1 + E2 calling 100 ms apart, E5 listening): one verdict of 81 ms in every pair, level +1 (+2 in two):
E1's call is not heard by anyone (2026-11-05-reach-glide), so only E2's train arrived; not a chord test.
chord_e2e4.txt (E2 + E4, both audible to E5; start chance 1 so nobody answers, decay 600 s, idle off):
one merged verdict of 186-187 ms (195 once) per pair, the level +1 in 8 of 12 pairs and +2 in 4.

Finding: the gate ("two answers 100 ms apart raise the level by 2") is not met reliably. The count rounds to the
nearest whole train: (186 + 62) / 125 = 1; two trains need >= 188 ms. A single glide reads 81-82 ms on the board
(its room tail is ~6 ms here), so the 50 ms margin makes the unit ~40 ms too big for the glide: with a unit near the
reading (85-90 ms) a 186 ms chord counts 2 and a lone train 1 (3 trains from ~220 ms). The +2 in pairs of 186-187 ms
came from something else (unexplained here: a third event, perhaps an answer; start chance 1 makes it rare).
For step 23.2 / the owner: the unit wants the glide's measured reading plus a small margin, not the 50 ms of the
165 ms train's contract. Not changed here (23.2 is the other session's).
