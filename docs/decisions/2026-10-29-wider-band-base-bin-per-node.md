# The network band widens to 8 bins (2950-4700 Hz) for a base bin per node; band shifting and a musical tone are tested after

Status: decided (owner 2026-10-29), not implemented; the roadmap item is BEH-009, sequenced at the milestone
cut-over (2026-10-26-milestone-installation-ready-node.md). Amends 2026-10-28-band-stays-after-the-sweep.md
through its revisit clause ("signals that need another range").
Date: 2026-10-29.

## Decision

1. **Wider band now.** The shipped network band grows from 5 bins (2950-3950 Hz) to 8 (2950-4700 Hz, bins 2950,
   3200, 3450, 3700, 3950, 4200, 4450, 4700), upward only, the bank's limit without a constant change
   (`FreqBandStream::kMaxBandBins`). The hop train stays 5 steps, 25 ms x 5, gap 10, on consecutive rising bins.
2. **A base bin per node.** The train starts on the node's base bin (0..3 on 8 bins) instead of wrapping around a
   rotation: the base is the node's note. Behavior owns it (random at boot; the roster's rotation becomes the
   override), the Node composes the request from it (the OUT-004 point of myspec 6.2), and the verdict carries the
   heard train's lowest bin (the bin track of DET-015, promoted into `OccurrenceVerdict` as myspec 6.1 asks;
   BEH-007's precondition). The alignment glides: on each own answer the base moves one bin toward the base last
   heard (owner 2026-10-29, "glide one bin per hearing", not a snap), so a node's pitch is heard moving toward its
   neighbour over several calls; at the automaton's rest the base scatters. The scatter range is BEH-009's design,
   after BEH-008.
3. **Then test the two ways to more range**, as experiments on the wider band: bands per node that wander until
   they hear something and then converge (docs/research/field-activity-response.md, Part 2), and a second,
   musical tone below 1 kHz (OUT-006). Which one ships is a later decision.

Glide rule confirmed (owner 2026-10-29, at the step 12-19 ordering): the base moves one bin toward the base last
heard on each own answer, as written above. The desk model (field-activity-response.md, "The glide and the scatter")
found that form slow to converge to unison and a per-hearing glide faster; the owner chose per answer. Step 18
watches for it: if the field does not settle on a shared base, revisit.

## Why

- The owner wants the base frequency per node to align over time (the note's Part 2). The start bin in a wider
  band is the one place it can live today without a new detection feature: the bin track already reads it.
- E004 (bench 2026-10-27-step11h-sweep24): the hop train hit 100 % at every position from 2250 to 5000 Hz; from
  1250 Hz up the noise (speech, music, flute, about an hour per position) did not reach the detector at all; the
  weak zone was 1500-3000 (65-89 %, moving between rounds, read as the room), below the shipped band. So the
  room is clean and strong above the current band and weak below it: widen upward.
- The cost in CPU is three more Goertzel bins per sample against a chain of ~39 of 62.5 us; memory does not scale
  with bins. The cost in range is small (4 base bins, a major third in quarter-tone steps), which is why 3
  follows.
- The owner is not worried about noise (2026-10-29); E004 is the evidence that it is not the problem in this room.

## Rules out

- Widening downward into 1500-3000 (weak there) or below 1250 (music reaches the detector; 40-60 % of trains lost
  under noise).
- More than 8 bins without re-measuring the per-sample budget (the bank constant).
- Keeping the rotation's wrap-around as the per-node variation once the base bin exists.
- Snapping onto the heard base in one call (the glide is the owner's choice; a snap is the fallback if the glide
  never converges inside a swell).

## Gate (for the item that lands it)

Analyzer hop10 at each base bin 0..3 on the 8-bin band >= 9/10 at the reference spacing; the plain chirp
rejected; the noise file 0 accepted; the Node battery (SELFTEST, 10 trains answered per base bin); the T2h
reference re-recorded and its diff explained (the band is a profile default, the fixtures were recorded at 5
bins); `PARAM DUMP` shows the new default. A short session, not a sweep.

## Revisit when

- The tryout room's hearing matrix shows a weak zone inside 2950-4700 (then the band moves, not shrinks).
- Experiment 3 lands one of the two ways to more range; the 8-bin band may then be the detection layer only.

## Source

Owner, 2026-10-29 ("Wider band now. Then we test with shifting the band or a second musical tone"; "not too
worried about noise"); docs/research/field-activity-response.md Part 2; E004.
