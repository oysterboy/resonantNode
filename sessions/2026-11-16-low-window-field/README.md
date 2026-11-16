# 2026-11-16 the field on the lowest window, nothing moving: detection rate

The owner's run: all nodes on the lowest frequency, no frequency shifting, the detection rate.

## Setup

- Gateway E0 (COM12) on the PC; R1, E2, E3 by radio (`node.radio_log 20`, `RB log full`). E1 did not answer.
- Live, not saved: `behavior.band_range_low_hz 250`, `band_range_high_hz 1250` with `band_per_node 1` (already on):
  one window 250-1250 Hz, five steps 250 Hz apart, a single base, so glide, scatter and wander cannot move it.
  Checked at the end: `base_bin 0`, `base_scatters 0`, `base_wanders 0` on all three. Everything else the stored
  params (the shipped field: start chance 85, level 200, spread 1000 ms).
- R1 and E2 played. **E3 listened only (muted):** unmuted it rebooted every 10-25 s, about 4 s after `baseline
  active` each time, before playing a train (`output.started 0`, 25 dropped audio buffers). Reading: a brownout when its
  amp starts. Not checked on its serial port. Muted, it ran the whole 988 s without a reboot.
- 877 s logged (`run1.log`), `boardview.py --boards E0=COM12 --radio R1,E2,E3 --poll "RB STATUS" --every 60`.
  Times in `changes.txt`.

## Result (`heard_by.py run1.log` -> `heard_by.txt`)

Share of a sender's trains that the listener logged as a valid detection within 0.4 s (trains overlapping a third
board's left out; in brackets: followed by any verdict):

| sender -> listener | valid |
|---|---|
| R1 -> E2 | 104 / 118 = 88 % (104) |
| E2 -> R1 | 112 / 125 = 90 % (112) |
| R1 -> E3 | 8 / 70 = 11 % (58) |
| E2 -> E3 | 0 / 80 = 0 % (73) |

- R1 and E2 hear each other about 9 times in 10 on 250-1250 Hz; every verdict they logged was valid (R1 102 / 102,
  E2 105 / 106). On 2500-3500 Hz earlier today R1 had heard nothing in 55 min, but E2 and E3 were muted then, so
  that says nothing about the window.
- E3 hears the trains (210 verdicts, most at strength ~23000, 134 ms) but calls 199 of them `single_pulse`, not
  the five-step train: the shape check fails at E3, not the level (its status showed the same 250-1250 window as
  the others). Why E3 sees one bin where R1 and E2 see the train is not found (its mic, its place). Only 11 valid.

## Open

- E3: the reboots when unmuted (supply, on its serial port) and the stored band / shape failure.
- E1 did not answer by radio.
- A longer run and the same on the shipped 2500-3500 window with the same two players, for the comparison.
