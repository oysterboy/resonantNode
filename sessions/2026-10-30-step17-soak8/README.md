# 2026-10-30/31: step 17, overnight soak on 13902862 (the 8-bin band, base bins, glide)

tools/bench/soakcycle.py --steps margin --chord E1,E4,E5 --noise-play, 8 h from 2026-10-30 23:22 to 07:22, 8 cycles.
Nodes E1 / E4 / E5 on 13902862 (band 2950-4700 stored, bases 0 / 3 / 1), Analyzer E2 and Emitter E3 on 1f0927a7
(the Analyzer's SEQ lines set band=2950-3950 themselves: its detection is configured as in every earlier soak).
No board reset.

| step | valid | note |
|---|---|---|
| hop10 0.5 FS / 0.35 FS (-3 dB) | 80 / 80, 80 / 80 | |
| hop10 0.25 FS (-6 dB) | 54 / 80 | 3, 1, 7, 4, 9 / 10 in cycles 1-5 (failed_amp), 10 / 10 in 6-8 |
| hop10 0.21 FS (-7.5 dB) | 37 / 80 | 0 in 1-4 (duration), 9-10 in 5-8 |
| hops 2 + 3 at -23 dB | 6 / 80 | failed_shape |
| hops 4 + 5 at -45 dB | 80 / 80 | |

Chords (answerers with level_max 0): +0 ms one train in 7 of 8 cycles (one cycle 1-2), +150 one, +250 two in
every cycle; every answer given. Music: 0 verdicts on E1 / E4 / E5 in all 8 cycles. No unexpected verdicts.

Why -6 dB lost margin against the earlier nights (2026-10-29-step14-soak47: 100 / 100 at the same received
strength, ~7,200): the amp inspection's p75 over the train's first 100 ms fell from ~2,700 to ~1,450-1,590 (the
medium limit is 1,500) while the loudest hop stayed as loud. The fall came between 21:31 and 22:31 on 2026-10-30
(2026-10-30-step14.2-soak10: 3,109 then 1,675) on an unchanged build, with nothing done to the boards. RAW per pulse
(2026-10-31-step17-margin-drop/raw_now against 2026-10-30-step14.2-boards/raw_after_a5193220, both_hop): 3450 Hz
14,746 -> 6,470 (-7 dB), 3950 Hz 3,153 -> 11,415 (+11 dB), the others within 2 dB: the room's comb moved on the desk
(something moved, or the night's temperature), and the hop that carried the first 100 ms is the one that dipped. Not
the firmware: the Analyzer's configuration printed at each SEQ start is identical. Lab E005.
