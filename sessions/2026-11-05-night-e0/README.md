# 2026-11-05 evening to 2026-11-06 morning: the Node field overnight, logged through E0

Five Nodes (E3 silent listener, E1 / E2 / E4 / E5 playing), the glide (75 ms), one window 2500-3500 Hz, start chance 85,
level_max 200, jitter 1000, wait 0, refractory 200, idle call 30 +- 10 s; node.radio_log 20 (saved), RB log full (live).
The radio gateway E0 (bare ESP32, COM12) on the PC; boardview.py --radio ... --poll "RB STATUS" wrote the logs
(night.log .. night4.log, gzipped; changes.txt lists every change with its time). The PC rebooted for a scheduled Windows
update at 03:55 (event 1074, "Service Pack, geplant"), which ended the logger (night4.log stops at 14783 s = 03:54), and
shut down unexpectedly at 09:15 (Kernel-Power 41, no bug check logged) until the owner re-plugged it at 09:44.

The boards did not notice: E0 is the only board on the PC's USB, the others are on chargers. Counters read at 09:56
(RB STATUS through E0, 37,960 s of audio uptime = 10.5 h since the 23:20 flash, E5 36,286 s):

| board | trains played (output.started) | valid trains heard | rests (level 200) | declined | dropped DMA buffers | resets |
|---|---:|---:|---:|---:|---:|---:|
| E1 | 9310 | 13554 | 85 | 730 | 0 | 0 |
| E2 | 8119 | 8975 | 58 | 693 | 0 | 0 |
| E3 (silent) | 0 | 17650 | 0 | 0 | 0 | 0 |
| E4 | 10454 | 13104 | 93 | 854 | 0 | 0 |
| E5 | 9106 | 12391 | 80 | 703 | 0 | 0 |

Radio (NET PING, same moment): queue_full 0, tx_failed 0 on every board.

night4.log (4.1 h, 23:48-03:54): E3 heard 7033 valid trains in 481 chains (gap 4 s): median 11 trains, maximum 97, 12 chains
over 50, the longest 37 s; chainlog.py on the emits (540 chains, median 20, max 184, 57 chains of one). What followed a chain's
last emit (the playing boards' decisions, 3 s): declined_by_level 96, invalid 93, resting 49, yielded 31, refractory 15,
rest_level 9. With radio_log 20 these lines are complete (2026-11-05-logcomplete-e0).
