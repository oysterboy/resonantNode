# 2026-10-30: step 14.2, short soak on a5193220 (stopped at 1.5 h by the owner's call)

tools/bench/soakcycle.py --steps margin --chord E1,E4,E5 --noise-play, 10 h planned from 21:29, stopped 23:00 after
two hourly cycles: the next build (the 8-bin band, step 17) carries every change under test, so the overnight soak
runs on it instead (owner 2026-10-30). Boards and settings as 2026-10-30-step14.2-boards. No board reset.

Margin: 0.5 / 0.35 / 0.25 FS and hops 2 + 3 at -23 dB, hops 4 + 5 at -45 dB 20 / 20; 0.21 FS 10 / 10 in cycle 1 and
0 / 10 in cycle 2 (the edge flips by the hour, lab E005). Chords (answerers with level_max 0): +0 and +150 ms one
train, +250 ms two, every answer given. Music: 0 verdicts on E1 / E4 / E5. No unexpected verdicts.
