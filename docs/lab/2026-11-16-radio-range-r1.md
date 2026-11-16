# Radio range, ESP-NOW: R1 against the gateway

Date: 2026-11-16. Data: `bench:sessions/2026-11-16-radio-range-r1` at `bench` commit `b94dfd1d` (setup, all 13
points, CSVs). Tool: `tools/bench/rangetest.py`. Gateway build `0f8ac9c9`, R1 build `3f16b56a`.

## Question

How far does the bench's ESP-NOW link carry between one gateway and one Node, and what does a wall cost? Asked
before the Nodes go into the building for the installation.

## What came out

- Straight line, no obstruction: the link holds to 15 m. 100 % of commands answered in every run; broadcast
  delivery 86 to 100 % with no trend against distance (15 m, three runs: 88.6, 93.9, 95.5 %). The limit in a clear
  line was not reached; longer runs are open.
- Concrete between at about 10 m: 90 % answered, broadcast 77 %. An open metal door with concrete corners at about
  17 m: 12.5 % answered, long replies mostly cut off.
- Broadcast delivery (`bcast_pct`) is the figure to watch: it falls before the answer rate does, because a command
  is broadcast three times.
- Unexplained, so it stays open: two runs at 15 m (15:25, 15:27) failed almost completely, minutes before three
  runs at the same distance passed, and nothing visibly differed. The check not made: gateway back at the first
  position, R1 at the same spot, two runs. Until then do not read the 15 m figure as a guarantee.
- Every distance is the owner's estimate, not tape-measured; the order and the notes are in the bench README.

## Open

- R1 kept beeping at idle with `output.silent 1` confirmed. What makes those beeps is not found.
- Not measured: height above the floor, antenna orientation, a second wall, distance beyond 15 m in a straight line.
