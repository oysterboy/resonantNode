# 2026-10-22/23: step 10c, first long-run block (overnight, 9.6 h)

Setup:
- Five nodes on Node 8ec0764, the step 6 / 10b layout, shipped defaults.
- `tools/bench/nodegroup.py --secs 34594 --status-every 120`, no kick: the network was driven only by its own idle
  calls (60 s +- 20 s).
- Quiet room, no playback. 22:23 to 08:00 Berlin time (owner's request: "a 10 h run until 8 am").
- summary.txt has nodegroup's summary.

| | result |
|---|---|
| chains ended within 30 s | **697 of 697** (median 3.7 s, max 6.0 s) |
| max emissions per minute (M = 100) | **50** |
| answers without another node's emission in the 2 s before (noise-started) | **0** of 12,726 |
| dropped DMA buffers / read errors / verdict-queue overflows | 0 / 0 / 0 in all 1,440 STATUS lines (288 per node) |
| resets | **0**: audio.uptime_ms rises without a break to 9.61 h on every node |
| deaf node | none: 340-424 valid verdicts per full hour on every node (hour 10 is 0.6 h) |
| audio deficit (s missing vs uptime) | constant all night (E1 1.8, E2 1.5, E3 1.4, E4 6.5, E5 1.5): boot-time only, no loss in the run |
| lowest free loop stack | 3,448 B on every node, unchanged |

Also met by this run: NODE-017's open check (Node real uptime past the 32-bit microsecond wrap). 9.6 h is
eight wraps of 71.6 min without a stop.

Not measurable here: per-board strength drift. RB STATUS carries no strength field and the verdict lines carry none
in log mode full. 10c's next block needs a strength field in RB STATUS, or periodic `RB summary` polls
(avg_strength), to cover the drift item of the gate.
