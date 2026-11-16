# 2026-11-16 radio range: one Node (R1) against one gateway (ESP-NOW)

Purpose: how far the bench's ESP-NOW link carries and what a wall costs, measured with `tools/bench/rangetest.py`
(main). No SEQ run, no `session.json`: this is a link measurement, not a detection run.

## Setup

- Gateway: node name E0, `role=gateway`, build `0f8ac9c9`, CH340 USB-UART on COM7 (it was COM6 until replugged;
  the number changes with the USB port). Laptop on the phone's hotspot: ESP-NOW does not use it (fixed
  `NET_CHANNEL` 1, no access point).
- Node: R1, build `3f16b56a`, powered from a power bank, muted live by the script (`output.silent 1`, not stored).
  R1 kept beeping at idle although `PARAM GET output.silent` returned 1; the source of the beeps is not found.
- Per point: 40 pings (`--pings 40`, gap 0.2 s), 3 long replies (`RB STATUS`), the node's counters read before and
  after. Run by piping the distance and note into the script, one point per call.
- No other ESPs nearby (owner). Distances are the owner's estimates ("about"), not measured with a tape.
- R1 stayed in one place from position C on; from D on the gateway was moved, and the distance is gateway to R1.
- First contact before the test, informal and not in the CSVs: pings from the basement to R1 three floors up
  answered 3 of 3.

## Results

Files: `range_R1_all.csv` (all rows, in run order), `range_R1_pos<X>.csv` (one row each). Times are 2026-11-16.

| Pos | Time | Distance | Path | Replies | Broadcast | Long whole |
|---|---|---|---|---|---|---|
| A | 15:19 | 1 m | line of sight | 100 % | 100 % | 3/3 |
| B | 15:21 | 4 m | gateway at start of section 1, R1 at the transition to section 2 | 100 % | 94.7 % | 3/3 |
| C | 15:25 | 15 m | gateway at B, line of sight | 12.5 % | no data | 3/3 |
| C2 | 15:27 | 15 m | same, R1 held still | 2.5 % | no data | 0/3 (no end 3) |
| D | 15:28 | ~11 m | gateway moved closer | 100 % | 90.9 % | 3/3 |
| E | 15:30 | ~12.5 m | straight line | 100 % | 86.4 % | 3/3 |
| F | 15:31 | 11 m | straight line (repeat of D) | 100 % | 93.2 % | 3/3 |
| G | 15:32 | 15 m | straight line | 100 % | 88.6 % | 3/3 |
| H | 15:33 | 15 m | straight line | 100 % | 93.9 % | 3/3 |
| I | 15:37 | ~17 m | from the anteroom, open metal door, concrete corners | 12.5 % | no data | 1/3 (no end 2) |
| J | 15:39 | ~10 m | gateway in section 2, concrete between | 90.0 % | 77.3 % | 3/3 |
| K | 15:40 | 7.5 m | straight line | 100 % | 97.0 % | 3/3 |
| L | 15:41 | 15 m | straight line | 100 % | 95.5 % | 3/3 |

Round trip 54 to 64 ms median (max 66 to 91 ms) in every row. `node_tx_failed` was 0 wherever it was read. "No data"
means the before and after `NET PING` that read R1's counters got no answer.

## What it shows

- In a straight line the link holds to 15 m: 100 % replies in every run, broadcast 86 to 100 % with no trend
  against distance (three runs at 15 m: 88.6, 93.9, 95.5 %). The limit in a clear line was not reached.
- Obstruction costs more than distance: concrete in between at about 10 m gave 90 % replies and 77 % broadcast;
  an open metal door with concrete corners at about 17 m gave 12.5 %.
- Unexplained: C and C2 (15 m, 15:25 and 15:27) failed almost completely, in the same stretch where G, H and L
  (15 m, 15:32 on) passed. The owner saw nothing different between the failing and passing runs. A transient
  (interference on channel 1, R1 just carried and set down) or an obstruction on the gateway-at-B path nobody
  noticed would both fit; the data does not separate them. The check not done: gateway back at B, R1 at the C
  spot, two runs.
- Only broadcast delivery (`bcast_pct`) shows a link running out before the reply rate does: the 3 repeats per
  command hide it in `reply_pct`.
