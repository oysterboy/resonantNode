# 2026-11-05 does a listening Node hear the short signal shapes?

E2 (COM3) plays, E3 (COM4, output.silent 1, RB log full) listens, both Node a7d6715c on one window 2000-3000 Hz
(band_per_node 1, range 2000-3000), boards side by side on the desk. shapedetect.py: each shape 10 x RB CALL,
1.4 s apart; a hit = an RB verdict line on E3 within 1.4 s. Pass A: E3's detection as is (scalar_min_duration_ms 92,
scalar_min_release_coverage_ms 80); pass B: 40 / 30 on E3, live, restored after. Shapes = the owner's listening
round (../2026-11-05-node-listen): "now" = today's 5 x 25 ms gap 10.

run.log is void: RB CALL goes through Behavior, and E2 rested after level_max (6) emissions. run2.log with
behavior.level_max 0 on E2:

| shape | owner | A verdicts / valid | B verdicts / valid | B dur_ms |
|---|---|---|---|---|
| now 5 x 25, gap 10 | - | 1 / 1 (E2 likely still resting) | 10 / 10 | 131 |
| 1: 8 x 10, gap 0 | yes | 0 / 0 | 10 / 0 single_pulse | 56 |
| 3: 8 x 25, gap 0 | yes | 10 / 10 | 10 / 10 | 148 |
| 4: 5 x 10, gap 0 | yes | 0 / 0 | 9 / 0 single_pulse | 56 |
| 5: 5 x 15, gap 0 | maybe | 0 / 0 | 10 / 10 | 83 |
| 6: 5 x 10, gap 5 | yes | 0 / 0 | 0 / 0 | - |
| 8: 3 x 15, gap 5 | yes | 0 / 0 | 10 / 10 | 55 |

Reading: today's detection hears 3 and today's shape. With the minimum duration at 40 ms, 5 and 8 pass as well; 1
and 4 are heard but the shape check reads no climb (10 ms a step is too fast for it: single_pulse); 6 is not heard
at all (10 ms pulses with gaps: no island reaches scalar_min_longest_island_ms 20). Not tested: what else passes
at 40 ms (clicks, typing, the room): a lower minimum is a detection retune and wants that check first.

Owner, after hearing 3, 8, 5 again (PC beeps 1/2/3, chirp385.py): 5 (5 x 15 ms, gap 0) sounds best.
E3 measured it at ~83 ms, under scalar_min_duration_ms 92: a minimum near 70 may be enough (less than the 40 tested).

## Shape 5 at a lower minimum, and false triggers (shape5.py, falsepos.py)

Hits, E2 plays 5 x 15 ms glide 15 times, E3 listens (shape5.log): 92/80 (scalar_min_duration_ms /
scalar_min_release_coverage_ms, today) 10/15; 70/80 9/15; 70/60 15/15 (dur ~82 ms). The coverage is the limit.

False triggers (falsepos.log): E2 at 92/80 and E3 at 70/60, both silent, window 2000-3000, side by side; the lab PC
played bench_audio.wav (435 s: drum and bass, female and male speech, flute) through the Studio One box at its
level. RB verdicts any / valid=1:

| section | 92/80 | 70/60 |
|---|---|---|
| silence 0-10 s | 1 / 0 | 1 / 0 |
| drum and bass | 1 / 0 | 1 / 0 |
| female speech | 0 / 0 | 1 / 0 |
| male speech | 0 / 0 | 1 / 0 |
| flute | 1 / 0 | 5 / 0 |

No valid false trigger at either; 70/60 lets more occurrences through (10 against 3), all rejected by the shape
check as single_pulse. One window, one noise file, no clicks or typing on purpose.

Host replay reference after the change: t2h_replay.d03f7189.txt (against 2026-11-01-step22.2-update16/t2h_replay.02e6a24.txt: accepted unchanged; tone2800_pos1_r0/r1 and radio_music_r1 gain one rejected candidate each).
