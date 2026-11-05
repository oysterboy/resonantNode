# 2026-11-05 the radio log on the boards, then the field one knob at a time

Image 5fde053b's content (node.radio_log; labelled d03f7189, built from the tree before the commit), later abea6bdb
(behavior.start_chance_pct) and 0b989164 (level_max up to 500). node.radio_log 5 on all; the board view with
--radio reads the charger boards' lines through the USB board. From 13:41 E3 (COM4) is the only USB board, silent,
the gateway; E1, E2, E4, E5 play on the power-only charger (E2 left the PC's USB after a drop). Every change with
its time: changes.txt; one board view log per setting (boards.log, boards2..17.log, in that order).

The owner's lesson of the day (one variable at a time): first everything on one window (2500-3500), then one knob
per round.

chainlog.py per segment (--quiet E3; chains split at 4 s; emits per chain median / max):

| log | setting (see changes.txt) | chains | median | max | what followed a chain's last emit, mostly |
|---|---|---|---|---|---|
| boards.log | per-node windows, level_max 4, jitter 800 | 7 | 2 | 2 | declined_by_level, invalid |
| boards3 | one window, level_max 0 | 3 | 5 | 5 | refractory (answers together: no jitter at level_max 0) |
| boards4-5 | + jitter 2500 (no effect at level_max 0), refractory 200 | 1-1 | 36-67 | | runs on |
| boards11 | start_chance 30, level_max 20, jitter 2000 | 13 | 5 | 18 | declined_by_level |
| boards12 | start_chance 50 | 3 | 11 | 24 | resting |
| boards13 | jitter 1000 | 7 | 2 | 15 | invalid, declined_by_level |
| boards14 | level_max 32 | 7 | 4 | 25 | invalid, resting |
| boards15 | level_max 50 | 57 | 7 | 44 | declined_by_level |
| boards16 | level_max 200, wait 0 | 10 | 4 | 15 | declined_by_level (the chance stays near the start at L 200) |
| boards17 | start_chance 85 (overnight from 14:53) | 42+ | 25 | 94 | (the run goes on) |

Found on the way: level_max 0 ignores answer_jitter_ms (decisions/2026-11-05-start-chance-and-chain-length.md); the
level counts every train heard, so L passes fast with four players; a chain ends when every hearer declines, so the
start chance sets the length more than L does. The radio log is lossy (log_dropped in NET STATUS): a chain's count
can miss an emit. E2 rebooted twice (13:5x power swap, 14:0x); the stored values then held.
