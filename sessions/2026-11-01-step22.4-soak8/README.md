# 2026-11-01: step 22.4, overnight soak on 02e6a24 (the band bank every 16 samples)

tools/bench/soakcycle.py --steps margin --chord E1,E4,E5 --noise-play, 8 h from 2026-11-01 01:16 to 09:16, 8 cycles.
Nodes E1 / E4 / E5 on 02e6a24 (FREQBAND_UPDATE_EVERY_SAMPLES 16), Analyzer E2 and Emitter E3 on the same build;
the tools' Analyzer band 2950-4700 (the 8-bin band, owner 2026-10-31). No board reset.

| step | valid | note |
|---|---|---|
| hop10 0.5 FS / 0.35 FS (-3 dB) | 80 / 80, 80 / 80 | |
| hop10 0.25 FS (-6 dB) | 80 / 80 | 2026-10-30-step17-soak8: 54 / 80 (failed_amp) |
| hop10 0.21 FS (-7.5 dB) | 30 / 80 | 7, 4, 2, 7, 5, 0, 3, 2 per cycle; all 50 rejects coverage_too_low |
| hops 2 + 3 at -23 dB | 0 / 80 | failed_shape, as designed (the soak before: 6 / 80) |
| hops 4 + 5 at -45 dB | 80 / 80 | |

Chords (answerers with level_max 0): +0 ms and +150 ms two trains, +250 ms three, in every cycle; every answer given
(11 / 11). Music: 0 verdicts on E1 / E4 / E5 in all 8 cycles. No unexpected verdicts.

Reading: the night is as good as the soaks at 4 or better from the operating point to -6 dB (the -6 dB loss of
2026-10-30 was the desk's comb, lab E005, not the interval). At -7.5 dB the loss is the one 22.3 measured and the
owner accepted (decisions/2026-11-01-band-update-16.md): coverage at 74-76 ms against the 80 minimum. Step 22's gate:
the CPU saving (22.1), the edge cases (22.3), this soak.
