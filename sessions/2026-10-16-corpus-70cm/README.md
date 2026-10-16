# 2026-10-16 capture corpus, 70 cm (tools/bench/rawcorpus.py)

Same capture setup as 2026-10-16-corpus-60cm (mode=both, 50 ms pre, 250 ms post, combined build, E3 0.5 FS). pos0 = the placement of 2026-10-16-hopcand-70cm, captured right after its cells.

| f (Hz) | pos0 |
|---|---|
| 2800 | 11,281 / 11,909 |
| 3200 | 36,933 / 36,674 |
| 3300 | 29,502 / 32,548 |
| 3800 | 33,824 / 33,234 |

3,200 Hz strong in the afternoon; the morning gate had the chirp 0/10 at this spacing. All three hop cells 10/10.

Evening, E2 reflashed to 5ba805d (shipped defaults, RAW forwards pulses/gap/hop), boards at the 2026-10-16-bandcheck-70cm
placement (`pos1`): the first hop captures through RAW, `hop5_p25g10_pos1_r0..2` (25 ms x 5, gap 10, 2950/3200/3450/3700/
3950, pre 50 / post 300 ms) and `tone3200_pos1_r0..1`. Host replay with band 2950-3950: all three hop captures accepted
(duration 200-201 ms, strength about 9,100), the chirp accepted, host amp feature within 1 Strength16 of the board's
column. (E3 had to be reset over USB after the E2 flash: its control link no longer acknowledged claims.)
