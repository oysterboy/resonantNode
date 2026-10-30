# E005 - How much headroom does the hop train have, in dB and in hops?

## Question

Step 13 (owner 2026-10-29): at the operating point (hop10 at the shipped band, Emitter 0.5 FS), how far can the
level drop, and how many hops can a train lose, before detection fails? Does Strength16 follow the weakest hop or
the mean (room-comb-and-margin.md, open question)?

## Status

answered for one placement (2026-10-29): 13.2 and 13.3 on the desk at 79 cm, 13.4 from the code and the attenuation
runs. Open: the other distances and a second spot (on site), and 13.1 (speaker or mic, the Emitter swap).
Design: docs/research/distance-range.md test 1, docs/research/room-comb-and-margin.md ("What would settle" 3).

## Setup

- E3 Emitter -> E2 Analyzer at 79 cm, the 2026-10-26 layout (remote session: nothing moved). Both on 0e8b3bd, which
  added PULSES `level=` (the train's peak in FS) and `att=` (dB per pulse) in the Emitter, forwarded by SEQ start;
  without them the tone output is bit for bit as before.
- hop10: 5 pulses of 25 ms, 10 ms gaps, 2950 / 3200 / 3450 / 3700 / 3950 Hz, band 2950-3950, TonalPulseScalar as
  shipped. 20 trials per cell, mode=detail.

## Protocol

13.2: the level ladder 0.5 / 0.35 / 0.25 / 0.21 / 0.18 / 0.15 / 0.12 FS (0.21 and 0.15 added after a 5-trial smoke
test put the edge between 0.25 and 0.125). 13.3: at 0.5 FS one middle hop down 10 / 15 / 20 / 30 dB, the first hop
down 20, two hops (2 and 4) down 15 and 20. One spot, one distance.

## Log

2026-10-29, level ladder (bench/sessions/2026-10-29-step13-levelladder):

- 20 / 20 at 0.5 and 0.35 FS (-3.1 dB), 11 / 20 at 0.25 (-6.0 dB), 0 / 20 at 0.21 (-7.5 dB) and below.
- Strength follows the level linearly: 14455 / 10126 / 7209 / 6024 / 5136 / 4218 / 3343 (median source_strength).
- What fails, in order as the level drops: at 0.25 the amp inspection (observed weak, medium required; the shape
  is still strong, all five bins rising), at 0.21 coverage_too_low, at 0.18 and below duration_too_short (only a
  couple of hops clear the release threshold, occurrences of 62-70 ms against the 92 ms minimum).

2026-10-29, per-hop attenuation at 0.5 FS (bench/sessions/2026-10-29-step13-hopatt):

- One hop lost costs nothing: the middle hop down 10, 15, 20 or 30 dB, or the first hop down 20 dB: 20 / 20 each.
  From 15 dB down the middle hop is gone from the shape (4 distinct bins, 2950-3700) and the occurrence is 113 ms
  instead of 217, still over the 92 ms minimum: the detector's stream bridges one missing 25 ms hop (the previous
  hop's ring-down plus the 10 ms release debounce).
- Two hops lost (2 and 4, 15 or 20 dB down): 0 / 20, every trial duration_too_short (69 ms pieces).
- Strength does not move when one hop is attenuated (14507-14524 against 14455); with hops 2 and 4 down it falls to
  ~11000, the loudest of the remaining hops.

2026-10-29, which two hops (owner: "I thought two lost ones still check out"; same session, three more runs, all
20 dB down):

- Hops 1 + 2 lost: 20 / 20 (3450-3700, 3 bins, 2 up). Hops 4 + 5 lost: 20 / 20 (strength 5521: 3700 / 3950 were
  the loudest hops at this spot). Three good hops in a row clear both the 92 ms duration and the shape rule's
  "3 distinct bins, 2 steps up" (decisions/2026-10-26-det015-bin-track-inspector.md).
- Hops 2 + 3 lost: 0 / 20, rejected by the shape rule: 2950 then 3700 is a jump larger than
  detection.shape_max_rising_step_bins, so it counts no step up (3 bins, 1 up, class weak).
- Hops 2 + 4 lost (above): 0 / 20 by duration, before the shape rule is reached.

2026-10-29, Strength16 (13.4): from the code, in band mode the scalar detector's stream is the strongest band bin's
value (DetectionRuntime, FrequencyTarget) and the occurrence's strength is that stream's peak (ScalarTransient
`_facts.peak`). So Strength16 follows the loudest hop, neither the weakest nor the mean; the attenuation runs above
show it. A dipped hop bites through duration and coverage, not strength.

## Findings

- Margin in dB at 79 cm: 100 % holds to -3 dB, about half at -6 dB, nothing at -7.5 dB. Call it 3-6 dB at this
  distance; the first gate to fail is the amp class (medium), then coverage, then duration.
- Margin in hops at 0.5 FS: any one hop of five may be lost entirely. Two lost hops pass only at an end of the
  train (1 + 2 or 4 + 5: three good hops in a row); two in the middle fail, by the step rule when adjacent (2 + 3)
  and by duration when apart (2 + 4).
- Strength16 is the loudest hop's level; it says nothing about a dipped hop. A site check that wants the weakest
  bin has to read the RAW capture per bin (sweepbins.py), as room-comb-and-margin.md recommends.
- Caveats: one spot at one distance; the comb can make the next spot 5 cm away different. The level ladder only
  bounds the range in the near regime (distance-range.md).

## Open threads

- The same at 70 and 110 cm and a second spot (on site, Sunday / Tuesday).
- 13.1: is the 3500-4500 Hz hump the speaker's or the mic's (swap the Emitter board).
- Whether the amp medium class, the first gate to fail, is where a lower installation level should be bought back
  (a tuning question, its own pass: not as a side effect).

## 2026-10-30: the margin over a day (soaks on b006c00)

bench/sessions/2026-10-30-step14-soak47b (11 hourly cycles, 09:49-20:55) and 2026-10-29-step14-soak47 (10 cycles
through the night), the 13.2 / 13.3 steps once an hour, 10 trials each:

- 0.5 FS, 0.35 FS (-3 dB) and hops 4 + 5 at -45 dB never missed (210 / 210 each).
- 0.25 FS (-6 dB) 198 / 210: two cycles of 21 at 3-5 / 10.
- 0.21 FS (-7.5 dB) and hops 2 + 3 at -23 dB flip as a whole from one hour to the next (0 / 10 or 10 / 10): 0.21 FS
  passed 8 / 100 through the night and morning, 45 / 110 the next day with every cycle from 17:00 at 10 / 10.
- Reading: the margin at the edge moves by a dB or more over a day on the same desk with nothing touched
  (temperature, the room's background, the Emitter's level). The operating point keeps its 6 dB; a site check at a
  single hour over-reads or under-reads the edge by that much. One more reason to read margin, not pass / fail, on
  site (15).
