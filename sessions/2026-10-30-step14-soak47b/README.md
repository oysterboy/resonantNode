# 2026-10-30: step 14, the soak on b006c00 (stopped at 11 h of 47 by the owner's call)

tools/bench/soakcycle.py --steps margin --chord --noise-play, 47 h planned, started 2026-10-30 09:49, stopped
2026-10-30 20:55 after 11 hourly cycles (owner 2026-10-30: "bestehe nicht auf den 48h"; the Nodes are next for
14.2's board check, and b006c00 is about to be replaced). Nodes E1 / E4 / E5 (COM8 / 10 / 11) on b006c00, Analyzer
E2 (COM3), Emitter E3; desk layout of 2026-10-26, Emitter to E2 79 cm. Restart of 2026-10-29-step14-soak47 with the
chord check fixed (rest rules off during it, answered_ab recorded). No board reset (no BUILD banner after the start).

Margin (Analyzer SEQ, 10 trials per step per cycle; cycles.csv):

| step | valid | note |
|---|---|---|
| hop10 0.5 FS | 110 / 110 | |
| hop10 0.35 FS (-3 dB) | 110 / 110 | |
| hop10 0.25 FS (-6 dB) | 98 / 110 | cycles 2 and 7 at 5 / 10 and 3 / 10 (duration_too_short) |
| hop10 0.21 FS (-7.5 dB) | 45 / 110 | 0 / 10 in cycles 1-6, 5 in 7, 10 / 10 in 8-11 |
| hops 2 + 3 at -23 dB | 55 / 110 | all or nothing per hour (failed_shape) |
| hops 4 + 5 at -45 dB | 110 / 110 | |

The edge steps flip as a whole within an hour: the margin drifts by a dB or more over the day (the evening cycles
pass at -7.5 dB; the earlier run, 2026-10-29-step14-soak47, passed it 8 / 100 through the night and morning). The
operating point (0.5 FS) and -3 dB never missed. No unexpected verdicts.

Chords (chord.csv, E1 listening, E4 / E5 answering 5 trains each): +0 and +150 ms one train per answer pair
(2 valid verdicts per train: the Emitter's and the chord) in every cycle; +250 ms two trains in 6 of 11 cycles,
merged into one in 2, partly lost in 3 (the room tail decides, as in step 14.1). Every answer pair answered
(answered_ab 11 / 11).

Music (noise.csv, bench_audio.wav once per cycle, every Node silent): 0 verdicts on E1 / E4 / E5 in every cycle.
