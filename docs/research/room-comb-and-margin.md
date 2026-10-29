# The room comb and the train's margin: what the band sweep measured, re-read per round and per bin

```text
Status:  exploring (2026-10-29)
Lab:     bench/sessions/2026-10-27-step11h-sweep24 (positions.csv per round; the 132 RAW hop captures through
         tools/bench/sweepbins.py); E004 (docs/lab/experiments/E004_band_sweep.md)
Related: frequency-tuning-sweep.md (the sweep's design); decisions/2026-10-28-band-stays-after-the-sweep.md (stands);
         distance-range.md (the same question along distance instead of frequency)
```

## Question (owner, 2026-10-29)

E004 read the weak zone 1500-3000 Hz as "the room, not the speaker" because its losses moved between rounds. Is
that convincing, and what does it mean for choosing and widening the band?

## What the per-round data says

The sweep's table averages 8-9 rounds per position. Per round (positions.csv, hop10 rows):

- **1500-2500 and 1750-2750 lost only in round 1.** 0/10 at 13:48 and 0/10 at 14:14 on the 27th, then 70/70 and
  80/80 through the night. Twenty of the sweep's forty-five losses are one event, not a null that wanders.
- **2000-3000 is the one position that keeps losing:** 2, 9, 3, 5, 10, 9, 10, 10, 7 of 10 across the rounds, with
  the lowest average strength of the sweep (4,500-5,900) and the only strength that falls through the night. The
  rejects are duration_too_short and failed_contrast: the gates at their margin, flipping trial by trial.
- **Round 1 was a different room.** Between round 1 (13:21-15:33) and round 2 (from 15:42, no gap in the log) the
  received level per hop bin shifted wholesale: 2750 Hz was a notch 15-35 dB deep in every round-1 capture that
  carried it and the strongest bin from round 2 on; 2950 Hz went the other way (85 dB, then 102); the shipped band's
  strength went from 9,600 to 13,000-14,600 at the same boundary. Nothing in the session notes a change; something in
  the room or on the table moved, or the owner left it.

## What the per-bin levels say (rounds 2-9)

`tools/bench/sweepbins.py` takes the Goertzel power at each hop bin over each RAW capture (one train of five
25 ms hops per visit), so the same absolute frequency is seen from every position that covers it:

```text
bin (Hz)   500  750  1000  1250  1500  1750  2000  2250  2500  2750  3000  3250  3500  3700  4000  4500  4750
median dB  108  118    96   105   102    95   100    95    86   102    96    90   102   110   103   102   100
spread dB    0    1    14     4     3     9     6    49    28     2     5    14     5     3     9     4    22
```

(median over rounds 2-9 and over the covering positions; spread is the largest round-to-round range at that bin.)

- **A comb.** Peaks near 1500, 2000, 2750, 3500-3700, 4000 and 4500; dips at 1000, 1750, 2500, 3250 and 4750, 10 to
  17 dB under their neighbours. The spacing of roughly 750 Hz is a reflection with a path difference around half a
  metre: a table or floor bounce at 79 cm, not a property of the spectrum.
- **The dips drift slowly and in one direction all night.** 1000 Hz fell from 109 to 91 dB, 2250 rose from 48 to 97,
  3450 from 77 to 101, 4750 wandered over 20 dB. A null edge is steep, so a small shift of the comb (air temperature,
  the speed of sound) moves a bin on the edge by tens of dB. This is the part that is really the room, and the same
  bins read in the Analyzer's strength: nearly every position's strength climbs a few percent through the night.
- **Why 2000-3000 fails and its neighbours pass.** Its five hops are 2000, 2250, 2500, 2750, 3000: two sit in the
  2250/2500 dip. 2250-3250 also carries two dipped hops (2500, 3250) and passed 80/80, 1750-2750 carries 1750 and
  2500 and passed 80/80 after round 1. The difference is not the count of dipped hops but the margin the rest leave:
  2000-3000 has the lowest strength of the three. The shipped band lives with the same thing at a higher margin: its
  3450 bin swung 23 dB across the rounds and it went 90/90, at strength 13,000-14,600.
- **One feature does not look like the room:** the broad, stable hump 3500-4500 Hz (3700 the strongest bin above
  1 kHz in every round and position, the whole hump within a few dB round to round). Broad and stationary across
  positions and rounds means the transducers (the MAX98357A speaker and the I2S mic), and it travels with the
  hardware. Likewise the roll-off below ~1250 Hz on the receive side (500/750 Hz read at the cap from the emitter's
  level, not from a flat response).

## Reading

So: half of "the room" holds, and the half that holds is the stronger finding.

- **The time variation is geometry.** A speaker's response does not change between 15:33 and 15:42 with nothing
  touched; a reflection comb does when anything does. Convincing.
- **"A weak zone 1500-3000 that comes and goes" does not hold.** Two thirds of it is a round-1 event before the
  comb shifted, the rest is one position with two hops in a stationary dip and the least margin. Not a zone in the
  spectrum; a place on this table.
- **"Room, not speaker" was the wrong fork for the decision.** The dips belong to one geometry and will sit at other
  frequencies at the site, differ node to node, drift through the day, and jump when a chair moves. The sweep saw
  all three on one table in 19 h. No 1 kHz slice is clean everywhere; any five-hop train will put one or two hops in
  a dip at some placement.
- **What travels is the broad shape:** the transducer hump at 3500-4500 Hz, the low roll-off, and the noise
  material carrying its energy below ~1250 Hz (where it reaches the detector and masks trains; above, it does not
  trigger the detector at all in this file). These do not depend on the room.
- **The design question is margin, not slice.** How much strength does the train carry over the gates, and how
  many of its hops can fall into a dip before duration and contrast flip? The sweep gives two points: fails with two
  dipped hops at ~4,700, passes with one swinging bin at ~14,000. Widening upward buys both more hops on the hardware
  hump and more spread, so one dip costs a smaller fraction of the train. That is the sound reason for the widening,
  and the band decision stands on it rather than on the weak zone.

## What would settle the open half

1. Ask what changed in the room between 15:33 and 15:42 on 2026-10-27. If nothing is known, the sensitivity is
   itself the finding.
2. Repeat hop10 at 2000-3000 with the emitter moved 10 cm sideways, or at 70 and 90 cm. If the losses move, the zone
   is this table; if they stay, the transducers have a real dip there and the zone is real.
3. A margin test, the generalisation of 2: at the shipped band, attenuate one and then two hops by 15 dB in the
   emitter (or move the pair until the capture shows two bins 15 dB down) and count hop10. That gives "hops a train
   may lose at strength S" directly, which is the number the site needs.

## Consequences for how bench results are read

- A single-placement run over hours is one geometry sampled over time, not the device's transfer function. A
  "band X is weak" finding needs a second placement before it goes into a decision file; a per-round and per-bin
  read (`sweepbins.py`) before it is averaged.
- Installation needs a short acceptance check per node pair: hop10, one RAW hop capture through `sweepbins.py`,
  and the count of bins 10 dB or more under their neighbours. Whether the margin is there in that room is the only
  question the room can answer.

## Open questions

- Is Strength16 itself comb-shaped, that is, does the Analyzer's strength follow the weakest or the mean hop? If the
  weakest, the margin test above is the right knob; if the mean, the gates' duration rule is where a dipped hop
  bites first (the round-1 rejects were 56 ms occurrences, two hops of five).
- Does the 3500-4500 hump belong to the speaker or the mic? A swap of the emitter board would say; it decides whether
  a node with a different speaker (OUT roadmap) keeps the margin.
