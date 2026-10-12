# 2026-10-12 SELFTEST on the status LED build (#29, 261f088)

E2 (COM3) flashed with the Node build at 261f088 (status LED on GPIO 2:
1 flash per accepted verdict, 2 per own chirp). SELFTEST PASS: mic, quiet
boot, amp 44.9 dB (tone 185k), own_chirp_suppressed 0 own verdicts. Tone
matches the last bd41423 run (182k, sessions/2026-10-12-selftest-e2-node-
skipfix run 6), so the -8 dB vs earlier runs is the rewiring, not the LED
change. The LED itself was not watched: visual gate of #29 still open.
