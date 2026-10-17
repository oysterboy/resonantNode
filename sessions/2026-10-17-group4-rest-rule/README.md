# 2026-10-17 four Nodes together with the rest rule, 600 s (tools/bench/nodegroup.py)

E1 (COM6), E2 (COM3), E3 (COM4), E4 (COM7) on main 2ddf2b4 (rest rule, TX-buffer fix), running freely: idle voice,
answers, rests. E5 (COM9, mic dead on its long cable) held silent. Placement as in 2026-10-17-pairmatrix (one row,
E2 -70- E3 -70- E1 -40- E4, E2/E3 20 cm higher). E3's output is unstable (supply/contact, see the selftest
session); run anyway at the owner's request, unattended.

| Node | answers | idle calls | valid verdicts | rests (field / own budget) |
|---|---|---|---|---|
| E1 | 101 | 9 | 126 | 16 / 1 |
| E2 | 55 | 10 | 79 | 4 / 0 |
| E3 | 55 | 17 | 65 | 6 / 0 |
| E4 | 88 | 12 | 106 | 13 / 0 |

Who heard whom (listener <- sender, verdicts attributed to the latest emission of another node 0.15-0.8 s before;
105 of 376 verdicts could not be attributed, overlapping emissions): E1 <- E4 74/100 (the 40 cm neighbour), every
other link 5-31 % (E3 <- E1 5/110 the weakest).

Group: 29-43 emissions per minute, steady over the ten minutes; the longest silences 18-20 s; the four nodes
spent 17 % of their time resting. No dropped DMA buffers on any node in any of the 36 status polls.

Reading: the rule ends the endless chain but the group is not quiet: with four nodes one is always out of rest and
an idle call restarts the exchange. Rests are frequent where contact is good (E1 and E4: 17 and 13), rare on the
weakly coupled E2/E3. "Rest above a level of traffic" holds per node; a quiet network would need a longer rest,
a lower field threshold, or a longer field window (all `RB BEHAV` / `FieldStateConfig` values).
