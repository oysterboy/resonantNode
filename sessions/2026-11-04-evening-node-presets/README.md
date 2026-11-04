# 2026-11-04 evening: Node presets, unattended

Five D-AMP boards (E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11), Node image built from ff6c612c, nobody in the
room. Each preset of tools/bench/nodepreset.py ran 480 s through tools/bench/nodegroup.py, live PARAM SET (not
saved); the boards were set silent afterwards. No resets or port drops.

| preset | emits | answers (heard) | idle | answers per idle | chains max | max emits/min |
|--------|------:|----------------:|-----:|-----------------:|-----------:|--------------:|
| low  (300-5000, lively)  | 59 | 29 | 30 | 1.0 | 2.7 s | 11 |
| deep (300-2000, lively)  | 66 | 50 | 16 | 3.1 | 3.6 s | 12 |
| calm (300-5000, sparse)  | 50 | 13 | 37 | 0.35 | 8.7 s | 13 |
| wide (950-4950, 2026-10-31) | 44 | 5 | 39 | 0.13 | 5.6 s | 7 |

Reading: the low band gets answered far more than the 950-4950 field; deep (three positions) is the most
conversational. Every chain ended within 30 s. nodegroup's "time to quiet" FAIL is not meaningful here: with idle
announcements every few seconds and no kicks, a 15 s quiet gap never comes; it is a measure for kicked runs.
Listening (how it sounds) is still open: the owner's session on 2026-11-05.
